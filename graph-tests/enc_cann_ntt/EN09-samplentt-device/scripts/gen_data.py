#!/usr/bin/python3
# coding=utf-8
# EN09-samplentt-device · 五段贯通 + 设备 SampleNTT golden
# Host 喂：ρ、σ、M4_ntt/M4_intt、γ、v；Â golden 由 Alg.7 从 ρ 生成（与设备同契约）
# 贯通：ρ→Â(SampleNTT) + σ→y→ŷ→t̂(Matvec 用设备 Â)→u→Pack(u)

import hashlib
import sys
import numpy as np
import os

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
K = 4
ZETA = 17
SEED_D = int(os.environ.get("SEED_D", "20260619"))
PREP_NONCE0 = int(os.environ.get("PREP_NONCE0", "0"))
np.random.seed(99)
os.environ["FIPS203_PRF_BACKEND"] = "shake256"

# Alg.7 几何（与笔记 / f203_alg7_layout 一致）
XOF_BYTES = 672
CAND_PAIRS = XOF_BYTES // 3  # 224


def hash_g_rho(d: bytes, kyber_k: int = K) -> bytes:
    """G(d‖byte(k)) 前 32B = ρ（矩阵种子）。"""
    return hashlib.sha3_512(d + bytes([kyber_k & 0xFF])).digest()[:32]


def sample_ntt_poly(rho: bytes, j: int, i: int) -> np.ndarray:
    """Alg.7 SampleNTT 单 poly：SHAKE128(ρ‖j‖i).squeeze(672) → rej → â[256]。"""
    msg = rho + bytes([j & 0xFF, i & 0xFF])
    buf = hashlib.shake_128(msg).digest(XOF_BYTES)
    out = []
    pos = 0
    for _t in range(CAND_PAIRS):
        c0, c1, c2 = buf[pos], buf[pos + 1], buf[pos + 2]
        pos += 3
        d1 = c0 + 256 * (c1 & 0x0F)
        d2 = (c1 >> 4) + 16 * c2
        if d1 < q and len(out) < n:
            out.append(d1)
        if d2 < q and len(out) < n:
            out.append(d2)
        if len(out) >= n:
            break
    if len(out) < n:
        raise SystemExit(f"SampleNTT rej short: got {len(out)} from {XOF_BYTES}B")
    return np.array(out[:n], dtype=np.int32)


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


C1_POLY_BYTES = n * 11 // 8  # 352
C2_BYTES = n * 5 // 8        # 160
C_BYTES = K * C1_POLY_BYTES + C2_BYTES  # 1568
UNIFIED_C = 41285357  # ⌊2^37/q⌋


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


def gen_wired_pipeline():
    """
    贯通链造数：
      ρ→Â(Alg.7) ；σ→CBD y→NTT ŷ→Matvec(Â,ŷ) t̂→INTT u→Pack(u,v_host)
    Â 不再 Host 随机；与设备 SampleNTT 同种子契约。
    """
    if ref not in ("kyber", "ml-kem", "mlkem"):
        raise ValueError(f"EN09 锁定 NTT_REF=kyber，收到 {ref!r}")
    if bench != K:
        raise ValueError(f"EN09 贯通要求 NTT_BENCH==K={K}，收到 bench={bench}")

    d = se.derand_bytes_from_seed(SEED_D, kyber_k=K)
    rho = hash_g_rho(d, K)
    sigma = se.hash_g_sigma(d, kyber_k=K)
    with open("./input/rho.bin", "wb") as f:
        f.write(rho)
    with open("./input/sigma.bin", "wb") as f:
        f.write(sigma)

    # ---- L0 SampleNTT：Â[i,j] = SampleNTT(ρ‖j‖i) ----
    a_hat = np.zeros((K, K, n), dtype=np.int32)
    for i in range(K):
        for j in range(K):
            a_hat[i, j] = sample_ntt_poly(rho, j, i)
    a_flat = a_hat.reshape(-1)
    a_flat.tofile("./output/golden_a_hat.bin")
    # 调试镜像：不再作为 Matvec 主路径 Host 输入
    a_flat.tofile("./output/wire_a_hat_expected.bin")
    print(f"[INFO] SampleNTT: rho=32B Â={a_flat.size} seed_d={SEED_D} (full k×k)")

    # ---- L1 Prep：σ + y ----
    rows = []
    for i in range(K):
        prf = se.prf_shake256(sigma, PREP_NONCE0 + i)
        rows.append(se.sample_poly_cbd2(prf))
    y = np.concatenate(rows).astype(np.int32)
    y.tofile("./output/golden_prep.bin")
    print(f"[INFO] Prep CBD: sigma=32B y={y.size} nonce0={PREP_NONCE0}")

    # ---- L2 NTT：ŷ = NTT(y) ----
    ntt_kyber.M = ntt_kyber.kyber_ntt_matrix(n=n, q=q)
    y_hat_list = []
    for p in range(K):
        poly = y[p * n : (p + 1) * n]
        y_hat_list.append(ntt_kyber.kyber_ntt(poly, n=n, q=q).astype(np.int32))
    y_hat = np.concatenate(y_hat_list)
    m4_ntt = pack_m4_from_dense(ntt_kyber.M)
    y_hat.tofile("./output/golden_ntt.bin")
    m4_ntt.tofile("./input/M4_ntt.bin")
    y.tofile("./output/wire_src_ntt_expected.bin")
    print(f"[INFO] NTT wired: y→ŷ elems={y_hat.size}")

    # ---- L3 Matvec：Â=SampleNTT；ŝ=ŷ ----
    gammas = gen_gammas()
    s_hat = y_hat.reshape(K, n)
    t_hat = np.zeros((K, n), dtype=np.int64)
    for j in range(K):
        for p in range(K):
            prod = multiply_ntts(a_hat[p, j], s_hat[j], gammas)
            t_hat[p] += prod.astype(np.int64)
    t_hat = np.mod(t_hat, q).astype(np.int32)
    t_flat = t_hat.reshape(-1)
    gammas.tofile("./input/gammas.bin")
    t_flat.tofile("./output/golden_matvec.bin")
    y_hat.tofile("./output/wire_s_hat_expected.bin")
    print(f"[INFO] Matvec wired: A=SampleNTT + ŷ→t̂ elems={t_flat.size}")

    # ---- L4 INTT ----
    u_list = []
    for p in range(K):
        z = inv.mlkem_inverse_ntt([int(v) for v in t_hat[p].tolist()])
        u_list.append(np.asarray(z, dtype=np.int32))
    u = np.concatenate(u_list)
    m_inv = inv.build_mlkem_inverse_matrix_int32()
    m4_intt = pack_m4_from_dense(m_inv)
    u.tofile("./output/golden_intt.bin")
    m4_intt.tofile("./input/M4_intt.bin")
    t_flat.tofile("./output/wire_src_intt_expected.bin")
    print(f"[INFO] INTT wired: t̂→u elems={u.size}")

    # ---- L5 Pack ----
    rng_v = np.random.RandomState(203)
    v = rng_v.randint(0, q, size=n, dtype=np.int32)
    c1 = bytearray(K * C1_POLY_BYTES)
    u_mat = u.reshape(K, n)
    for p in range(K):
        comp = np.array([compress_unified(int(x), 11) for x in u_mat[p]], dtype=np.int32)
        c1[p * C1_POLY_BYTES : (p + 1) * C1_POLY_BYTES] = byte_encode_d11(comp)
    comp_v = np.array([compress_unified(int(x), 5) for x in v], dtype=np.int32)
    c2 = byte_encode_d5(comp_v)
    c = bytes(c1) + c2
    v.tofile("./input/v_pack.bin")
    u.tofile("./output/wire_u_pack_expected.bin")
    with open("./output/golden_pack.bin", "wb") as f:
        f.write(c)
    print(f"[INFO] Pack wired: u=INTT v=Host c={len(c)} (du=11 dv=5)")


if __name__ == "__main__":
    gen_tiling()
    gen_wired_pipeline()
    print("[OK] EN09 SampleNTT+wired-pipeline inputs written")
