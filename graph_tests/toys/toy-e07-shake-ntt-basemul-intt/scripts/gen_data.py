#!/usr/bin/env python3
# coding=utf-8
"""
gen_data.py — E07：ntt256 输入 + SHAKE + ĝ + Minv + INTT(MultiplyNTTs(NTT(src),ĝ)) golden。

流水线：run.sh 编译后、kernel 前。
写 input/tiling.bin、src.bin、M4.bin、Minv4.bin、g.bin 与
output/golden.bin（INTT∘basemul）、golden_ntt.bin、golden_basemul.bin、shake_golden.bin。

语义：NTT/INTT = ntt256 矩阵正/逆（≠ Tag5T）；
      basemul = FIPS Alg.11/12（γ=K_ALG11_GAMMAS）；
      SHAKE = hashlib.shake_256(b"abc").digest(32)。
"""
import hashlib
import os
import struct
import sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import ntt_sim_kyber  # noqa: E402

n = 256
Q = 3329
np.random.seed(42)

# 与 vendor/basemul_scalar/alg11_gammas.h / multiplyntts gen_data 同步
K_ALG11_GAMMAS = [
    17, 3312, 2761, 568, 583, 2746, 2649, 680, 1637, 1692, 723, 2606, 2288, 1041, 1100, 2229,
    1409, 1920, 2662, 667, 3281, 48, 233, 3096, 756, 2573, 2156, 1173, 3015, 314, 3050, 279,
    1703, 1626, 1651, 1678, 2789, 540, 1789, 1540, 1847, 1482, 952, 2377, 1461, 1868, 2687, 642,
    939, 2390, 2308, 1021, 2437, 892, 2388, 941, 733, 2596, 2337, 992, 268, 3061, 641, 2688,
    1584, 1745, 2298, 1031, 2037, 1292, 3220, 109, 375, 2954, 2549, 780, 2090, 1239, 1645, 1684,
    1063, 2266, 319, 3010, 2773, 556, 757, 2572, 2099, 1230, 561, 2768, 2466, 863, 2594, 735,
    2804, 525, 1092, 2237, 403, 2926, 1026, 2303, 1143, 2186, 2150, 1179, 2775, 554, 886, 2443,
    1722, 1607, 1212, 2117, 1874, 1455, 1029, 2300, 2110, 1219, 2935, 394, 885, 2444, 2154, 1175,
]


def barrett_red_coeff(x: int) -> int:
    """Barrett 模约化到 [0,q)，与设备标量路径一致。"""
    q = Q
    t = x + (q & (x >> 31))
    t1 = (t * 78) >> 18
    x = t - t1 * q
    t2 = (x * 5039) >> 24
    x = x - t2 * q
    x = x - (q & ~((x - q) >> 31))
    return int(x)


def alg12_base_case_multiply(a0: int, a1: int, b0: int, b1: int, gamma: int):
    a1b1 = barrett_red_coeff(a1 * b1)
    c0 = barrett_red_coeff(a0 * b0 + a1b1 * gamma)
    c1 = barrett_red_coeff(a0 * b1 + a1 * b0)
    return c0, c1


def alg11_multiply_ntts(f: np.ndarray, g: np.ndarray) -> np.ndarray:
    """Alg.11 MultiplyNTTs：AoS 逐对 BaseCaseMultiply。"""
    h = np.zeros(n, dtype=np.int32)
    for i in range(n // 2):
        c0, c1 = alg12_base_case_multiply(
            int(f[i * 2]), int(f[i * 2 + 1]), int(g[i * 2]), int(g[i * 2 + 1]), K_ALG11_GAMMAS[i]
        )
        h[i * 2] = c0
        h[i * 2 + 1] = c1
    return h


def make_g_poly() -> np.ndarray:
    """玩具 ĝ：(13*i+7)%q — 与 multiplyntts 探针同模式。"""
    return np.array([(13 * i + 7) % Q for i in range(n)], dtype=np.int32)


def modinv(a: int, p: int) -> int:
    return pow(int(a) % p, p - 2, p)


def mat_inv_mod(A: np.ndarray, p: int) -> np.ndarray:
    """Gauss-Jordan 求 A^{-1} (mod p)。"""
    A = A.astype(object).copy()
    nn = A.shape[0]
    I = np.eye(nn, dtype=object)
    Aug = np.concatenate([A, I], axis=1)
    for col in range(nn):
        piv = None
        for r in range(col, nn):
            if Aug[r, col] % p != 0:
                piv = r
                break
        if piv is None:
            raise ValueError("singular matrix mod q")
        if piv != col:
            Aug[[col, piv]] = Aug[[piv, col]]
        inv = modinv(Aug[col, col], p)
        Aug[col] = [(v * inv) % p for v in Aug[col]]
        for r in range(nn):
            if r == col:
                continue
            f = Aug[r, col] % p
            if f:
                Aug[r] = [(Aug[r, c] - f * Aug[col, c]) % p for c in range(2 * nn)]
    return Aug[:, nn:].astype(np.int64)


def pack_m4_limbs(m: np.ndarray) -> np.ndarray:
    """将 int32 矩阵按 7bit×4 肢打包为 int8 串（与 ntt256 M4.bin 一致）。"""
    m = m.astype(np.int32)
    m0 = ((m >> 0) & 0x7F).astype(np.int8).reshape(-1)
    m1 = ((m >> 7) & 0x7F).astype(np.int8).reshape(-1)
    m2 = ((m >> 14) & 0x7F).astype(np.int8).reshape(-1)
    m3 = ((m >> 21) & 0x7F).astype(np.int8).reshape(-1)
    return np.concatenate((m0, m1, m2, m3)).astype(np.int8)


def intt_via_minv(h: np.ndarray, Minv: np.ndarray) -> np.ndarray:
    """矩阵逆 INTT：h @ Minv (mod q)；与设备 Mmad(Minv)+Merge+Barrett 语义对齐。"""
    return (h.astype(np.int64) @ Minv.astype(np.int64) % Q).astype(np.int32)


def main() -> None:
    in_dir = os.path.join(ROOT, "input")
    out_dir = os.path.join(ROOT, "output")
    os.makedirs(in_dir, exist_ok=True)
    os.makedirs(out_dir, exist_ok=True)

    payload = struct.pack("<ii", 0, n)
    payload += b"\x00" * (64 - len(payload))
    with open(os.path.join(in_dir, "tiling.bin"), "wb") as f:
        f.write(payload)

    input_x, golden_ntt = ntt_sim_kyber.gen_golden_data(n=n, q=3329, g=17)
    input_x = input_x.astype(np.int32)
    golden_ntt = golden_ntt.astype(np.int32)
    input_x.tofile(os.path.join(in_dir, "src.bin"))
    golden_ntt.tofile(os.path.join(out_dir, "golden_ntt.bin"))

    g_hat = make_g_poly()
    g_hat.tofile(os.path.join(in_dir, "g.bin"))

    golden_h = alg11_multiply_ntts(golden_ntt, g_hat)
    golden_h.tofile(os.path.join(out_dir, "golden_basemul.bin"))

    m = ntt_sim_kyber.M.astype(np.int32)
    pack_m4_limbs(m).tofile(os.path.join(in_dir, "M4.bin"))

    Minv = mat_inv_mod(m, Q)
    # 自检：NTT→INTT 还原
    rec = intt_via_minv(golden_ntt, Minv)
    assert np.array_equal(rec, input_x % Q), "Minv failed NTT roundtrip"
    pack_m4_limbs(Minv.astype(np.int32)).tofile(os.path.join(in_dir, "Minv4.bin"))

    # 设备 out = INTT(MultiplyNTTs(NTT(src), ĝ))
    golden_out = intt_via_minv(golden_h, Minv)
    golden_out.tofile(os.path.join(out_dir, "golden.bin"))

    shake_golden = hashlib.shake_256(b"abc").digest(32)
    with open(os.path.join(out_dir, "shake_golden.bin"), "wb") as f:
        f.write(shake_golden)

    test01 = ntt_sim_kyber.ntt_test01(n=n, q=3329, g=17, f=input_x)
    assert np.all(test01 == golden_ntt)
    print(f"[gen_data] E07 ntt256+basemul+INTT+shake under {in_dir} (NTT/INTT≠Tag5T; Alg.11/12)")


if __name__ == "__main__":
    main()
