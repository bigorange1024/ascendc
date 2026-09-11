#!/usr/bin/python3
# coding=utf-8
# EN05-prep-sample-realbrick · 五段造数
# L1：σ + golden y（Alg.8 CBD η=2 × K；PRF=SHAKE256，与设备一致）
# L2/L4：沿用 EN02 正向 NTT / 逆向 INTT（独立造数；布局与 Prep 同构）
# L3：真 NTT 域 4×4×1 内积（Â Host 喂 = SampleNTT 本刀不做）
# L5：Pack 桩随机块

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


def gen_stub_block(path: str, seed: int):
    """写出桩段输入 [bench*n] int32，范围 [0,q)。"""
    rng = np.random.RandomState(seed)
    src = rng.randint(0, q, size=bench * n, dtype=np.int32)
    src.tofile(path)
    print(f"[INFO] stub src {path}: elems={src.size}")


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
        raise ValueError(f"EN05 锁定 NTT_REF=kyber，收到 {ref!r}")
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
    gen_stub_block("./input/src_pack.bin", seed=203)
    print("[OK] EN05 five-segment inputs written")
