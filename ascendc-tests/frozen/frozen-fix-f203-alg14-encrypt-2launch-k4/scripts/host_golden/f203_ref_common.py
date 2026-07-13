#!/usr/bin/env python3
"""
f203_ref_common.py — host_golden 共用 FIPS 203 参考（禁止 liboqs）。

含 NTT/INTT Stage123、Compress/ByteEncode、Alg.11 basemul 等。
"""
from __future__ import annotations

import hashlib
import re
from pathlib import Path

import numpy as np

K = 4
N = 256
Q = 3329
HALF_N = N // 2
LIMBS = 4
MAT_C_PLANAR_ROWS = K * LIMBS * 2
LIMB_BITS = 6
XOF_BYTES = 672
CAND_PAIRS = XOF_BYTES // 3

CASE = Path(__file__).resolve().parent.parent.parent
LUT_HDR = CASE / "compute/ntt_r/thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h"

GAMMAS = np.array(
    [
        17, 3312, 2761, 568, 583, 2746, 2649, 680, 1637, 1692, 723, 2606, 2288, 1041, 1100, 2229, 1409, 1920,
        2662, 667, 3281, 48, 233, 3096, 756, 2573, 2156, 1173, 3015, 314, 3050, 279, 1703, 1626, 1651, 1678,
        2789, 540, 1789, 1540, 1847, 1482, 952, 2377, 1461, 1868, 2687, 642, 939, 2390, 2308, 1021, 2437, 892,
        2388, 941, 733, 2596, 2337, 992, 268, 3061, 641, 2688, 1584, 1745, 2298, 1031, 2037, 1292, 3220, 109,
        375, 2954, 2549, 780, 2090, 1239, 1645, 1684, 1063, 2266, 319, 3010, 2773, 556, 757, 2572, 2099, 1230,
        561, 2768, 2466, 863, 2594, 735, 2804, 525, 1092, 2237, 403, 2926, 1026, 2303, 1143, 2186, 2150, 1179,
        2775, 554, 886, 2443, 1722, 1607, 1212, 2117, 1874, 1455, 1029, 2300, 2110, 1219, 2935, 394, 885, 2444,
        2154, 1175,
    ],
    dtype=np.int32,
)


def mod_q_i64(x: int) -> int:
    rem = x % Q
    if rem < 0:
        rem += Q
    return int(rem)


def barrett_red(x: int) -> int:
    t = x + (Q & (x >> 31))
    t1 = (t * 78) >> 18
    x = t - t1 * Q
    t2 = (x * 5039) >> 24
    x = x - t2 * Q
    x = x - (Q & ~((x - Q) >> 31))
    return int(x)


def multiply_ntts(f: np.ndarray, g: np.ndarray) -> np.ndarray:
    h = np.zeros(N, dtype=np.int32)
    for i in range(N // 2):
        gamma = int(GAMMAS[i])
        a0, a1 = int(f[2 * i]), int(f[2 * i + 1])
        b0, b1 = int(g[2 * i]), int(g[2 * i + 1])
        a1b1 = barrett_red(a1 * b1)
        h[2 * i] = barrett_red(a0 * b0 + a1b1 * gamma)
        h[2 * i + 1] = barrett_red(a0 * b1 + a1 * b0)
    return h


def load_lut_t_i8(mode: str) -> np.ndarray:
    symbol = "kMlkemLimb6Ntt_T_i8" if mode == "ntt" else "kMlkemLimb6Intt_T_i8"
    txt = LUT_HDR.read_text(encoding="utf-8")
    i0 = txt.index(symbol)
    i1 = txt.index("{", i0)
    i2 = txt.index("};", i1)
    nums = [int(x) for x in re.findall(r"-?\d+", txt[i1 + 1 : i2])]
    return np.array(nums, dtype=np.int8).reshape(N, 512)


def a_hat_offset(p: int, j: int) -> int:
    return (p * K + j) * N


def a_hat_offset_at(p: int, j: int) -> int:
    return (j * K + p) * N


def planar_row(slot: int, limb: int, half: int) -> int:
    return half * (K * LIMBS) + slot * LIMBS + limb


def encode_compact(polys: np.ndarray) -> np.ndarray:
    s0 = np.zeros((2 * K, N), dtype=np.int8)
    for lp in range(K):
        for r in range(N):
            v = int(polys[lp, r]) % Q
            s0[lp, r] = (v >> LIMB_BITS) & 0x3F
            s0[K + lp, r] = v & 0x3F
    return s0


def mat_c_tmp_golden(s0: np.ndarray, lut: np.ndarray) -> tuple[np.ndarray, ...]:
    le = lut[:, 0:N:2]
    lo = lut[:, 1:N:2]
    he = lut[:, N:512:2]
    ho = lut[:, N + 1 : 512 : 2]
    c_lo_even = (s0.astype(np.int32) @ le.astype(np.int32)).astype(np.int32)
    c_lo_odd = (s0.astype(np.int32) @ lo.astype(np.int32)).astype(np.int32)
    c_hi_even = (s0.astype(np.int32) @ he.astype(np.int32)).astype(np.int32)
    c_hi_odd = (s0.astype(np.int32) @ ho.astype(np.int32)).astype(np.int32)
    return c_lo_even, c_lo_odd, c_hi_even, c_hi_odd


def pack_bank(c_le, c_lo, c_he, c_ho, poly_base: int, k_polys: int, out: np.ndarray) -> None:
    for lp in range(k_polys):
        hi_r = poly_base + lp
        lo_r = K + poly_base + lp
        slot = poly_base + lp
        out[planar_row(slot, 0, 0), :] = c_le[hi_r, :]
        out[planar_row(slot, 1, 0), :] = c_lo[hi_r, :]
        out[planar_row(slot, 2, 0), :] = c_le[lo_r, :]
        out[planar_row(slot, 3, 0), :] = c_lo[lo_r, :]
        out[planar_row(slot, 0, 1), :] = c_he[hi_r, :]
        out[planar_row(slot, 1, 1), :] = c_ho[hi_r, :]
        out[planar_row(slot, 2, 1), :] = c_he[lo_r, :]
        out[planar_row(slot, 3, 1), :] = c_ho[lo_r, :]


def pack_mat_c_planar(c_le, c_lo, c_he, c_ho) -> np.ndarray:
    out = np.zeros((MAT_C_PLANAR_ROWS, HALF_N), dtype=np.int32)
    pack_bank(c_le, c_lo, c_he, c_ho, 0, K, out)
    return out


def stage31_mod(raw: np.ndarray) -> np.ndarray:
    raw64 = raw.astype(np.int64)
    q = np.int64(Q)
    t = np.where(raw64 >= 0, raw64 // q, -((-raw64) // q))
    rem = raw64 - q * t
    rem = rem - q * (rem >= q).astype(np.int64)
    rem = rem + q * (rem < 0).astype(np.int64)
    return rem.astype(np.int32)


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
    """k=4 polyvec Stage123 NTT 或 INTT。"""
    if polys.shape != (K, N):
        raise ValueError(f"expected ({K},{N}) got {polys.shape}")
    lut = load_lut_t_i8(mode)
    s0 = encode_compact(polys)
    c_le, c_lo, c_he, c_ho = mat_c_tmp_golden(s0, lut)
    mat_planar = pack_mat_c_planar(c_le, c_lo, c_he, c_ho)
    dst = np.zeros((K, N), dtype=np.int32)
    for slot in range(K):
        dst[slot] = merge_planar_poly(mat_planar, slot)
    return dst


def compress_d_scalar(u: int, d: int) -> int:
    u = int(u) % Q
    if d == 5:
        d0 = u * 1290176
        return ((d0 + (1 << 27)) >> 27) & 0x1F
    if d == 11:
        d0 = u * 5284526080
        d0 = (d0 + (1 << 32)) >> 33
        return int(d0 & 0x7FF)
    raise ValueError(f"unsupported d={d}")


def byte_encode_d(F: np.ndarray, d: int) -> bytes:
    """FIPS 203 Alg.5：256 个 d-bit 整数 → 32*d 字节。"""
    bits: list[int] = []
    mask = (1 << d) - 1
    for val in F:
        a = int(val) & mask
        for j in range(d):
            bits.append(a & 1)
            a >>= 1
    out = bytearray((len(bits) + 7) // 8)
    for i, b in enumerate(bits):
        if b:
            out[i >> 3] |= 1 << (i & 7)
    return bytes(out)


def poly_byte_encode12(poly: np.ndarray) -> bytes:
    out = bytearray(384)
    for i in range(128):
        t0 = int(poly[2 * i]) % Q
        t1 = int(poly[2 * i + 1]) % Q
        out[3 * i] = t0 & 0xFF
        out[3 * i + 1] = ((t0 >> 8) & 0x0F) | ((t1 & 0x0F) << 4)
        out[3 * i + 2] = (t1 >> 4) & 0xFF
    return bytes(out)


def pack_ciphertext(u: np.ndarray, v: np.ndarray) -> bytes:
    """ml_kem_1024：c₁=4×ByteEncode₁₁(Compress₁₁(u))，c₂=ByteEncode₅(Compress₅(v))。"""
    c1 = bytearray(1408)
    for p in range(K):
        comp = np.array([compress_d_scalar(int(x), 11) for x in u[p]], dtype=np.int32)
        c1[p * 352 : (p + 1) * 352] = byte_encode_d(comp, 11)
    comp_v = np.array([compress_d_scalar(int(x), 5) for x in v], dtype=np.int32)
    c2 = byte_encode_d(comp_v, 5)
    return bytes(c1) + c2


def embed_message(v: np.ndarray, m: bytes) -> np.ndarray:
    out = v.copy()
    half_q = (Q + 1) // 2
    for i in range(32):
        for j in range(8):
            idx = 8 * i + j
            bit = (m[i] >> j) & 1
            out[idx] = mod_q_i64(int(out[idx]) + half_q * bit)
    return out
