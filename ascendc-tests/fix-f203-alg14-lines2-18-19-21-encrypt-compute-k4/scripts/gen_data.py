#!/usr/bin/env python3
# coding=utf-8
"""Alg.14 行 18–19 可行性 golden（自包含；k=4）。

行 18: ŷ ← NTT(y)
行 19: u ← INTT(Σ_j A[j,p]∘ŷ[j]) + e₁  （Âᵀ∘ŷ 的 û 部分，不含行 21）
"""
from __future__ import annotations

import os
import re
import struct
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
sys.path.insert(0, _SCRIPT_DIR)
from mlkem_ref import stage31_mod  # noqa: E402

_NTT_LUT_HDR = os.path.normpath(
    os.path.join(_CASE_DIR, "../../thirdparty/ntt_study/include/mlkem/stable/transpose_mlkem_luts_i8.h")
)

N = 256
HALF_N = N // 2
K = 4
K_PER_AIV = 2
M_ROWS = 2 * K
LIMBS = 4
MAT_C_PLANAR_ROWS = K * LIMBS * 2
LIMB_MASK = 0x3F
LIMB_BITS = 6
Q = 3329
SEED = 20260706

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


def load_lut_t_i8(mode: str) -> np.ndarray:
    symbol = "kMlkemLimb6Ntt_T_i8" if mode == "ntt" else "kMlkemLimb6Intt_T_i8"
    with open(_NTT_LUT_HDR, encoding="utf-8") as f:
        txt = f.read()
    i0 = txt.index(symbol)
    i1 = txt.index("{", i0)
    i2 = txt.index("};", i1)
    body = txt[i1 + 1 : i2]
    nums = [int(x) for x in re.findall(r"-?\d+", body)]
    expect = N * 512
    if len(nums) != expect:
        raise SystemExit(f"LUT {symbol} size {len(nums)} != {expect}")
    return np.array(nums, dtype=np.int8).reshape(N, 512)


def lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def encode_compact(batch: np.ndarray, s0: np.ndarray) -> None:
    for lp in range(batch.shape[0]):
        for r in range(N):
            v = int(batch[lp, r]) % Q
            s0[lp, r] = (v >> LIMB_BITS) & LIMB_MASK
            s0[K + lp, r] = v & LIMB_MASK


def encode_s0(polys: np.ndarray) -> np.ndarray:
    s0 = np.zeros((M_ROWS, N), dtype=np.int8)
    encode_compact(polys, s0)
    return s0


def mat_c_tmp_golden(s0: np.ndarray, lut: np.ndarray):
    le = lut[:, 0:N:2]
    lo = lut[:, 1:N:2]
    he = lut[:, N:512:2]
    ho = lut[:, N + 1 : 512 : 2]
    c_lo_even = (s0.astype(np.int32) @ le.astype(np.int32)).astype(np.int32)
    c_lo_odd = (s0.astype(np.int32) @ lo.astype(np.int32)).astype(np.int32)
    c_hi_even = (s0.astype(np.int32) @ he.astype(np.int32)).astype(np.int32)
    c_hi_odd = (s0.astype(np.int32) @ ho.astype(np.int32)).astype(np.int32)
    return c_lo_even, c_lo_odd, c_hi_even, c_hi_odd


def planar_row(slot: int, limb: int, half: int) -> int:
    return half * (K * LIMBS) + slot * LIMBS + limb


def pack_bank_planar(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, poly_base: int, k_polys: int, out: np.ndarray) -> None:
    for lp in range(k_polys):
        hi_r = poly_base + lp
        lo_r = K + poly_base + lp
        slot = poly_base + lp
        out[planar_row(slot, 0, 0), :] = c_lo_even[hi_r, :]
        out[planar_row(slot, 1, 0), :] = c_lo_odd[hi_r, :]
        out[planar_row(slot, 2, 0), :] = c_lo_even[lo_r, :]
        out[planar_row(slot, 3, 0), :] = c_lo_odd[lo_r, :]
        out[planar_row(slot, 0, 1), :] = c_hi_even[hi_r, :]
        out[planar_row(slot, 1, 1), :] = c_hi_odd[hi_r, :]
        out[planar_row(slot, 2, 1), :] = c_hi_even[lo_r, :]
        out[planar_row(slot, 3, 1), :] = c_hi_odd[lo_r, :]


def pack_mat_c_planar(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd) -> np.ndarray:
    out = np.zeros((MAT_C_PLANAR_ROWS, HALF_N), dtype=np.int32)
    pack_bank_planar(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, 0, K_PER_AIV, out)
    pack_bank_planar(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, K_PER_AIV, K_PER_AIV, out)
    return out


def merge_planar_poly(mat_planar: np.ndarray, slot: int) -> np.ndarray:
    hh = mat_planar[planar_row(slot, 0, 0)].astype(np.int64)
    lh = mat_planar[planar_row(slot, 1, 0)].astype(np.int64)
    hl = mat_planar[planar_row(slot, 2, 0)].astype(np.int64)
    ll = mat_planar[planar_row(slot, 3, 0)].astype(np.int64)
    raw_lo = hh * 4096 + (hl + lh) * 64 + ll
    hh = mat_planar[planar_row(slot, 0, 1)].astype(np.int64)
    lh = mat_planar[planar_row(slot, 1, 1)].astype(np.int64)
    hl = mat_planar[planar_row(slot, 2, 1)].astype(np.int64)
    ll = mat_planar[planar_row(slot, 3, 1)].astype(np.int64)
    raw_hi = hh * 4096 + (hl + lh) * 64 + ll
    out = np.zeros(N, dtype=np.int32)
    out[:HALF_N] = stage31_mod(raw_lo.astype(np.int32))
    out[HALF_N:] = stage31_mod(raw_hi.astype(np.int32))
    return out


def stage123_transform(polys: np.ndarray, mode: str) -> np.ndarray:
    lut = load_lut_t_i8(mode)
    s0 = encode_s0(polys)
    c_le, c_lo, c_he, c_ho = mat_c_tmp_golden(s0, lut)
    mat_planar = pack_mat_c_planar(c_le, c_lo, c_he, c_ho)
    dst = np.zeros((K, N), dtype=np.int32)
    for slot in range(K):
        dst[slot] = merge_planar_poly(mat_planar, slot)
    return dst


def barrett_red_coeff(x: int) -> int:
    q = Q
    t = x + (q & (x >> 31))
    t1 = (t * 78) >> 18
    x = t - t1 * q
    t2 = (x * 5039) >> 24
    x = x - t2 * q
    x = x - (q & ~((x - q) >> 31))
    return int(x)


def multiply_ntts(f: np.ndarray, g: np.ndarray) -> np.ndarray:
    h = np.zeros(N, dtype=np.int32)
    for i in range(N // 2):
        gamma = K_ALG11_GAMMAS[i]
        a0, a1 = int(f[2 * i]), int(f[2 * i + 1])
        b0, b1 = int(g[2 * i]), int(g[2 * i + 1])
        a1b1 = barrett_red_coeff(a1 * b1)
        h[2 * i] = barrett_red_coeff(a0 * b0 + a1b1 * gamma)
        h[2 * i + 1] = barrett_red_coeff(a0 * b1 + a1 * b0)
    return h


def a_hat_offset_jp(j: int, p: int) -> int:
    return (j * K + p) * N


def main() -> None:
    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    rng = np.random.default_rng(SEED)
    y = rng.integers(0, Q, size=(K, N), dtype=np.int32)
    a_hat = rng.integers(0, Q, size=(K, K, N), dtype=np.int32)  # 存 A[p,j] 于 [p,j]
    e1 = rng.integers(-2, 3, size=(K, N), dtype=np.int32)

    y_hat = stage123_transform(y, "ntt")

    u_ntt = np.zeros((K, N), dtype=np.int32)
    for p in range(K):
        acc = np.zeros(N, dtype=np.int64)
        for j in range(K):
            # A[j,p]：矩阵行 j 列 p → flat(j,p)
            ap = a_hat[j, p]
            prod = multiply_ntts(ap, y_hat[j])
            acc += prod.astype(np.int64)
        u_ntt[p] = acc % Q

    u_time = stage123_transform(u_ntt, "intt")
    u = ((u_time.astype(np.int64) + e1.astype(np.int64)) % Q).astype(np.int32)

    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    lut_ntt_even = lut_planar_stacked(lut_ntt, True)
    lut_ntt_odd = lut_planar_stacked(lut_ntt, False)
    lut_intt_even = lut_planar_stacked(lut_intt, True)
    lut_intt_odd = lut_planar_stacked(lut_intt, False)

    y.tofile(os.path.join(_CASE_DIR, "input", "y.bin"))
    a_hat.reshape(K * K, N).tofile(os.path.join(_CASE_DIR, "input", "a_hat.bin"))
    e1.tofile(os.path.join(_CASE_DIR, "input", "e1.bin"))
    lut_ntt_even.tofile(os.path.join(_CASE_DIR, "input", "lut_ntt_even_stacked.bin"))
    lut_ntt_odd.tofile(os.path.join(_CASE_DIR, "input", "lut_ntt_odd_stacked.bin"))
    lut_intt_even.tofile(os.path.join(_CASE_DIR, "input", "lut_intt_even_stacked.bin"))
    lut_intt_odd.tofile(os.path.join(_CASE_DIR, "input", "lut_intt_odd_stacked.bin"))

    y_hat.tofile(os.path.join(_CASE_DIR, "output", "golden_y_hat.bin"))
    u_ntt.tofile(os.path.join(_CASE_DIR, "output", "golden_u_ntt.bin"))
    u.tofile(os.path.join(_CASE_DIR, "output", "golden_u.bin"))

    tiling = struct.pack("<iii", N, K, 3)
    with open(os.path.join(_CASE_DIR, "input", "tiling.bin"), "wb") as f:
        f.write(tiling.ljust(64, b"\x00"))

    print(f"[gen_data] k={K} y_hat + u_ntt + u golden written (seed={SEED})")


if __name__ == "__main__":
    main()
