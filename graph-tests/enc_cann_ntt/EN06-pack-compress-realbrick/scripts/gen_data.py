#!/usr/bin/python3
# coding=utf-8
# EN06-pack-compress-realbrick · 五段造数
# L1：σ + golden y（Alg.8 CBD η=2 × K；PRF=SHAKE256，与设备一致）
# L2/L4：沿用 EN02 正向 NTT / 逆向 INTT（独立造数；布局与 Prep 同构）
# L3：真 NTT 域 4×4×1 内积（Â Host 喂 = SampleNTT 本刀不做）
# L5：真 Pack — u/v → Compress₁₁/₅ + ByteEncode → golden c[1568]

import sys
import numpy as np
import os

# shared golden：Alg.8 CBD
_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
_SE = os.path.join(_REPO, "library", "shared", "fips203_se_sample")
sys.path.insert(0, _SE)
import golden_se_sampling as se  # noqa: E402

import ntt_kyber
import gen_inverse_matrix as inv

n = int(os.environ.get("NTT_N", "256"))
bench = int(os.environ.get("NTT_BENCH", "4"))
q = int(os.environ.get("NTT_Q", "3329"))
ref = os.environ.get("NTT_REF", "kyber").lower()
# Matvec / Prep 已锁 ML-KEM-1024
K = 4
ZETA = 17
SEED_D = int(os.environ.get("SEED_D", "20260619"))
PREP_NONCE0 = int(os.environ.get("PREP_NONCE0", "0"))
np.random.seed(99)
# 设备 Prep 固定 SHAKE256 PRF；golden 必须同后端
os.environ["FIPS203_PRF_BACKEND"] = "shake256"


def barrett_params(modulus):
    """Barrett 参数 b_k / b_mu，写入 tiling 供核内约化。"""
    b_k = 0
    temp_q = int(modulus)
    while temp_q > 0:
        temp_q >>= 1
        b_k += 1
    b_mu = (1 << (2 * b_k)) // int(modulus)
    return b_k, b_mu


def pack_m4_from_dense(m: np.ndarray) -> np.ndarray:
    """dense int32 矩阵 → 作者包 M4 四平面 int8（与 EN01/EN02 一致）。"""
    m = m.astype(np.int32)
    m0 = ((m >> 0) & 0x7F).astype(np.int8).reshape(-1)
    m1 = ((m >> 7) & 0x7F).astype(np.int8).reshape(-1)
    m2 = ((m >> 14) & 0x7F).astype(np.int8).reshape(-1)
    m3 = ((m >> 21) & 0x7F).astype(np.int8).reshape(-1)
    return np.concatenate((m0, m1, m2, m3), dtype=np.int8)


def bitrev7(i: int) -> int:
    """7-bit 位反转（FIPS 203 BitRev7）。"""
    r = 0
    for b in range(7):
        r = (r << 1) | ((i >> b) & 1)
    return r


def gen_gammas() -> np.ndarray:
    """γ_i = ζ^{2·BitRev7(i)+1} mod q，i=0..127。"""
    out = np.zeros(n // 2, dtype=np.int32)
    for i in range(n // 2):
        out[i] = pow(ZETA, 2 * bitrev7(i) + 1, q)
    return out


def mod_q(x: int) -> int:
    r = int(x) % q
    if r < 0:
        r += q
    return r


def multiply_ntts(a: np.ndarray, b: np.ndarray, gammas: np.ndarray) -> np.ndarray:
    """FIPS 203 Alg.11/12：paired basemul → 长度 N 的积多项式。"""
    c = np.zeros(n, dtype=np.int32)
    for i in range(n // 2):
        a0 = int(a[2 * i])
        a1 = int(a[2 * i + 1])
        b0 = int(b[2 * i])
        b1 = int(b[2 * i + 1])
        g = int(gammas[i])
        c[2 * i] = mod_q(a0 * b0 + a1 * b1 * g)
        c[2 * i + 1] = mod_q(a0 * b1 + a1 * b0)
    return c


def gen_tiling():
    os.makedirs("input", exist_ok=True)
    os.makedirs("output", exist_ok=True)
    b_k, b_mu = barrett_params(q)
    r28 = (1 << 28) % int(q)
    print(f"[INFO] tiling: n={n}, bench={bench}, q={q}, b_k={b_k}, b_mu={b_mu}, r28={r28}")
    tiling_np = np.array([n, bench, q, b_k, b_mu, r28], dtype=np.int32)
    tiling_np.tofile("./input/tiling.bin")


# Pack 密文外形（ML-KEM-1024）
C1_POLY_BYTES = n * 11 // 8  # 352
C2_BYTES = n * 5 // 8        # 160
C_BYTES = K * C1_POLY_BYTES + C2_BYTES  # 1568
UNIFIED_C = 41285357  # ⌊2^37/q⌋，与设备 Compress 同源


def compress_unified(u: int, d: int) -> int:
    """统一整数 Compress_d：(C·u + 2^(36-d)) >> (37-d) 再 mask d bit。"""
    u = int(u) % q
    if u < 0:
        u += q
    y = (UNIFIED_C * u + (1 << (36 - d))) >> (37 - d)
    return int(y) & ((1 << d) - 1)


def byte_encode_d5(comp: np.ndarray) -> bytes:
    """Alg.5 ByteEncode_5：8 系数 → 5B/组。"""
    out = bytearray(C2_BYTES)
    for i in range(n // 8):
        t = [int(comp[8 * i + j]) & 0x1F for j in range(8)]
        base = i * 5
        out[base + 0] = 0xFF & ((t[0] >> 0) | (t[1] << 5))
        out[base + 1] = 0xFF & ((t[1] >> 3) | (t[2] << 2) | (t[3] << 7))
        out[base + 2] = 0xFF & ((t[3] >> 1) | (t[4] << 4))
        out[base + 3] = 0xFF & ((t[4] >> 4) | (t[5] << 1) | (t[6] << 6))
        out[base + 4] = 0xFF & ((t[6] >> 2) | (t[7] << 3))
    return bytes(out)


def byte_encode_d11(comp: np.ndarray) -> bytes:
    """Alg.5 ByteEncode_11：8 系数 → 11B/组。"""
    out = bytearray(C1_POLY_BYTES)
    for j in range(n // 8):
        t = [int(comp[8 * j + k]) & 0x7FF for k in range(8)]
        base = j * 11
        out[base + 0] = (t[0] >> 0) & 0xFF
        out[base + 1] = ((t[0] >> 8) | ((t[1] << 3) & 0xFF)) & 0xFF
        out[base + 2] = ((t[1] >> 5) | ((t[2] << 6) & 0xFF)) & 0xFF
        out[base + 3] = (t[2] >> 2) & 0xFF
        out[base + 4] = ((t[2] >> 10) | ((t[3] << 1) & 0xFF)) & 0xFF
        out[base + 5] = ((t[3] >> 7) | ((t[4] << 4) & 0xFF)) & 0xFF
        out[base + 6] = ((t[4] >> 4) | ((t[5] << 7) & 0xFF)) & 0xFF
        out[base + 7] = (t[5] >> 1) & 0xFF
        out[base + 8] = ((t[5] >> 9) | ((t[6] << 2) & 0xFF)) & 0xFF
        out[base + 9] = ((t[6] >> 6) | ((t[7] << 5) & 0xFF)) & 0xFF
        out[base + 10] = (t[7] >> 3) & 0xFF
    return bytes(out)


def gen_pack_compress():
    """
    L5：随机 u[K,N]、v[N] ∈ [0,q) → Compress₁₁/₅ + ByteEncode → golden c。
    本刀独立造数（不接 INTT 输出）；覆盖全密文 1568B。
    """
    rng = np.random.RandomState(203)
    u = rng.randint(0, q, size=(K, n), dtype=np.int32)
    v = rng.randint(0, q, size=n, dtype=np.int32)
    c1 = bytearray(K * C1_POLY_BYTES)
    for p in range(K):
        comp = np.array([compress_unified(int(x), 11) for x in u[p]], dtype=np.int32)
        c1[p * C1_POLY_BYTES : (p + 1) * C1_POLY_BYTES] = byte_encode_d11(comp)
    comp_v = np.array([compress_unified(int(x), 5) for x in v], dtype=np.int32)
    c2 = byte_encode_d5(comp_v)
    c = bytes(c1) + c2
    u.reshape(-1).tofile("./input/u_pack.bin")
    v.tofile("./input/v_pack.bin")
    with open("./output/golden_pack.bin", "wb") as f:
        f.write(c)
    print(f"[INFO] Pack: u={u.size} v={v.size} c={len(c)} (du=11 dv=5)")


def gen_prep_cbd():
    """
    L1：SEED_D → d → G → σ；PRF_SHAKE256(σ, nonce0..+K-1) → CBD η=2 → y[K,N]。
    布局与 NTT src [bench=K,n] 同构（行主序）。
    """
    d = se.derand_bytes_from_seed(SEED_D, kyber_k=K)
    sigma = se.hash_g_sigma(d, kyber_k=K)
    rows = []
    for i in range(K):
        prf = se.prf_shake256(sigma, PREP_NONCE0 + i)
        rows.append(se.sample_poly_cbd2(prf))
    y = np.concatenate(rows).astype(np.int32)
    with open("./input/sigma.bin", "wb") as f:
        f.write(sigma)
    y.tofile("./output/golden_prep.bin")
    print(f"[INFO] Prep CBD: sigma=32B y={y.size} seed_d={SEED_D} nonce0={PREP_NONCE0} PRF=SHAKE256")


def gen_forward_ntt():
    """L2：时间域 src → 正向 NTT golden；M4_ntt 为正向 dense digit。"""
    if ref not in ("kyber", "ml-kem", "mlkem"):
        raise ValueError(f"EN06 锁定 NTT_REF=kyber，收到 {ref!r}")
    src, golden = ntt_kyber.gen_kyber_data(n=n, q=q, bench=bench)
    m4 = pack_m4_from_dense(ntt_kyber.M)
    src.astype(np.int32).tofile("./input/src_ntt.bin")
    golden.astype(np.int32).tofile("./output/golden_ntt.bin")
    m4.tofile("./input/M4_ntt.bin")
    print(f"[INFO] NTT: src={src.size} golden={golden.size} M4={m4.size}")


def gen_inverse_intt():
    """L4：NTT 域 src → INTT golden；M4_intt 由 mlkem_inverse_ntt 矩阵 digit 打包。"""
    rng = np.random.RandomState(101)
    src_list = []
    golden_list = []
    for _ in range(bench):
        x = rng.randint(0, q, size=n, dtype=int)
        z = inv.mlkem_inverse_ntt([int(v) for v in x.tolist()])
        src_list.append(np.asarray(x, dtype=np.int32))
        golden_list.append(np.asarray(z, dtype=np.int32))
    src = np.concatenate(src_list)
    golden = np.concatenate(golden_list)
    m_inv = inv.build_mlkem_inverse_matrix_int32()
    m4 = pack_m4_from_dense(m_inv)
    src.tofile("./input/src_intt.bin")
    golden.tofile("./output/golden_intt.bin")
    m4.tofile("./input/M4_intt.bin")
    print(f"[INFO] INTT: src={src.size} golden={golden.size} M4={m4.size} "
          f"matrix_max={int(m_inv.max())}")


def gen_matvec_innerproduct():
    """
    L3：行主序 Â[K,K,N]、ŝ[K,N]、γ[N/2] → t̂[K,N]。
    flat(p,j,c)=(p·K+j)·N+c；t̂[p]=mod_q(Σ_j Â[p,j]∘ŝ[j])。
    """
    rng = np.random.RandomState(303)
    a_hat = rng.randint(0, q, size=(K, K, n), dtype=np.int32)
    s_hat = rng.randint(0, q, size=(K, n), dtype=np.int32)
    gammas = gen_gammas()
    t_hat = np.zeros((K, n), dtype=np.int64)
    for j in range(K):
        for p in range(K):
            prod = multiply_ntts(a_hat[p, j], s_hat[j], gammas)
            t_hat[p] += prod.astype(np.int64)
    t_hat = np.mod(t_hat, q).astype(np.int32)

    a_flat = a_hat.reshape(-1)
    s_flat = s_hat.reshape(-1)
    t_flat = t_hat.reshape(-1)
    a_flat.tofile("./input/a_hat.bin")
    s_flat.tofile("./input/s_hat.bin")
    gammas.tofile("./input/gammas.bin")
    t_flat.tofile("./output/golden_matvec.bin")
    print(f"[INFO] Matvec: a={a_flat.size} s={s_flat.size} t={t_flat.size} gammas={gammas.size}")


if __name__ == "__main__":
    gen_tiling()
    gen_prep_cbd()
    gen_forward_ntt()
    gen_matvec_innerproduct()
    gen_inverse_intt()
    gen_pack_compress()
    print("[OK] EN06 five-segment inputs written")
