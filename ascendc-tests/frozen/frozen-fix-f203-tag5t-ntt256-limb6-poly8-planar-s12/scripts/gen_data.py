#!/usr/bin/env python3
# coding=utf-8
"""
平面 mat_c Stage1+2 golden（fix-f203-tag5t-ntt256-limb6-poly8-planar-s12）。
Stage2：偶/奇 LUT 列分乘 + 行重排 → mat_c_planar [64,128]（hh|lh|hl|ll 各占一行）。
"""
import os
import re
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_SHARED = os.path.normpath(os.path.join(_SCRIPT_DIR, "../../../../library/shared"))
_NTT_LUT_HDR = os.path.normpath(
    os.path.join(_SCRIPT_DIR, "../../../../thirdparty/ntt_study/include/mlkem/stable/transpose_mlkem_luts_i8.h")
)
sys.path.insert(0, _SHARED)

import merged_kyber_fixed_poly  # noqa: E402

N = 256
HALF_N = N // 2
K_POLYS = 8
K_POLYS_PER_AIV = 4
M_ROWS = 2 * K_POLYS
LIMBS = 4
MAT_C_PLANAR_ROWS = K_POLYS * LIMBS * 2
LUT_COLS = 512
LIMB_MASK = 0x3F
LIMB_BITS = 6
Q = 3329


def load_lut_t_i8() -> np.ndarray:
    with open(_NTT_LUT_HDR, encoding="utf-8") as f:
        txt = f.read()
    anchor = "kMlkemLimb6Ntt_T_i8"
    i0 = txt.index(anchor)
    i1 = txt.index("{", i0)
    i2 = txt.index("};", i1)
    body = txt[i1 + 1 : i2]
    nums = [int(x) for x in re.findall(r"-?\d+", body)]
    expect = N * LUT_COLS
    if len(nums) != expect:
        raise SystemExit(f"[gen_data] LUT size {len(nums)} != {expect}")
    return np.array(nums, dtype=np.int8).reshape(N, LUT_COLS)


def lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    """[512,128] int8：上/下各 256 行，列取 LUT 偶数或奇数列。"""
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def s0_hi_row(sub_core: int, lp: int) -> int:
    return sub_core * (2 * K_POLYS_PER_AIV) + lp


def s0_lo_row(sub_core: int, lp: int) -> int:
    return sub_core * (2 * K_POLYS_PER_AIV) + K_POLYS_PER_AIV + lp


def planar_row(p: int, limb: int, half: int) -> int:
    return half * (K_POLYS * LIMBS) + p * LIMBS + limb


def encode_tag5t_rt_polybatch(batch: np.ndarray) -> np.ndarray:
    k = batch.shape[0]
    s0 = np.zeros((2 * k, N), dtype=np.int8)
    for sub in range(k // K_POLYS_PER_AIV):
        for lp in range(K_POLYS_PER_AIV):
            p = sub * K_POLYS_PER_AIV + lp
            for r in range(N):
                v = int(batch[p, r]) % Q
                s0[s0_hi_row(sub, lp), r] = (v >> LIMB_BITS) & LIMB_MASK
                s0[s0_lo_row(sub, lp), r] = v & LIMB_MASK
    return s0


def mat_c_tmp_golden(s0: np.ndarray, lut: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    le = lut[:, 0:N:2]
    lo = lut[:, 1:N:2]
    he = lut[:, N:512:2]
    ho = lut[:, N + 1 : 512 : 2]
    c_lo_even = (s0.astype(np.int32) @ le.astype(np.int32)).astype(np.int32)
    c_lo_odd = (s0.astype(np.int32) @ lo.astype(np.int32)).astype(np.int32)
    c_hi_even = (s0.astype(np.int32) @ he.astype(np.int32)).astype(np.int32)
    c_hi_odd = (s0.astype(np.int32) @ ho.astype(np.int32)).astype(np.int32)
    return c_lo_even, c_lo_odd, c_hi_even, c_hi_odd


def pack_mat_c_planar(
    c_lo_even: np.ndarray,
    c_lo_odd: np.ndarray,
    c_hi_even: np.ndarray,
    c_hi_odd: np.ndarray,
) -> np.ndarray:
    out = np.zeros((MAT_C_PLANAR_ROWS, HALF_N), dtype=np.int32)
    for p in range(K_POLYS):
        sub = p // K_POLYS_PER_AIV
        lp = p % K_POLYS_PER_AIV
        hi_r = s0_hi_row(sub, lp)
        lo_r = s0_lo_row(sub, lp)
        out[planar_row(p, 0, 0), :] = c_lo_even[hi_r, :]
        out[planar_row(p, 1, 0), :] = c_lo_odd[hi_r, :]
        out[planar_row(p, 2, 0), :] = c_lo_even[lo_r, :]
        out[planar_row(p, 3, 0), :] = c_lo_odd[lo_r, :]
        out[planar_row(p, 0, 1), :] = c_hi_even[hi_r, :]
        out[planar_row(p, 1, 1), :] = c_hi_odd[hi_r, :]
        out[planar_row(p, 2, 1), :] = c_hi_even[lo_r, :]
        out[planar_row(p, 3, 1), :] = c_hi_odd[lo_r, :]
    return out


def mat_c_interleaved_golden(s0: np.ndarray, lut: np.ndarray) -> np.ndarray:
    c_lo = (s0.astype(np.int32) @ lut[:, :N].astype(np.int32)).astype(np.int32)
    c_hi = (s0.astype(np.int32) @ lut[:, N:].astype(np.int32)).astype(np.int32)
    return np.concatenate([c_lo, c_hi], axis=0)


def interleaved_to_planar(mat_c: np.ndarray) -> np.ndarray:
    """从权威交错 mat_c [32,256] 推导平面布局，用于交叉校验。"""
    out = np.zeros((MAT_C_PLANAR_ROWS, HALF_N), dtype=np.int32)
    c_lo = mat_c[0:16]
    c_hi = mat_c[16:32]
    for p in range(K_POLYS):
        sub = p // K_POLYS_PER_AIV
        lp = p % K_POLYS_PER_AIV
        hi_r = s0_hi_row(sub, lp)
        lo_r = s0_lo_row(sub, lp)
        out[planar_row(p, 0, 0), :] = c_lo[hi_r, 0::2]
        out[planar_row(p, 1, 0), :] = c_lo[hi_r, 1::2]
        out[planar_row(p, 2, 0), :] = c_lo[lo_r, 0::2]
        out[planar_row(p, 3, 0), :] = c_lo[lo_r, 1::2]
        out[planar_row(p, 0, 1), :] = c_hi[hi_r, 0::2]
        out[planar_row(p, 1, 1), :] = c_hi[hi_r, 1::2]
        out[planar_row(p, 2, 1), :] = c_hi[lo_r, 0::2]
        out[planar_row(p, 3, 1), :] = c_hi[lo_r, 1::2]
    return out


def gen_tiling(mix_pass: int = 0):
    os.makedirs("input", exist_ok=True)
    os.makedirs("output", exist_ok=True)
    np.array([N, K_POLYS, mix_pass], dtype=np.int32).tofile("./input/tiling.bin")


if __name__ == "__main__":
    mix_pass = int(os.environ.get("PLANAR_MIX_PASS", os.environ.get("TAG5T_MIX_PASS", "0")))
    gen_tiling(mix_pass)

    src_batch = np.tile(merged_kyber_fixed_poly.FIXED_POLY.reshape(1, -1), (K_POLYS, 1))
    lut = load_lut_t_i8()
    lut_even_stacked = lut_planar_stacked(lut, even=True)
    lut_odd_stacked = lut_planar_stacked(lut, even=False)
    lut_even_stacked.tofile("./input/lut_even_stacked.bin")
    lut_odd_stacked.tofile("./input/lut_odd_stacked.bin")
    src_batch.tofile("./input/src.bin")

    s0_ref = encode_tag5t_rt_polybatch(src_batch)
    c_lo_e, c_lo_o, c_hi_e, c_hi_o = mat_c_tmp_golden(s0_ref, lut)
    mat_planar_ref = pack_mat_c_planar(c_lo_e, c_lo_o, c_hi_e, c_hi_o)

    mat_interleaved = mat_c_interleaved_golden(s0_ref, lut)
    mat_planar_from_interleaved = interleaved_to_planar(mat_interleaved)
    cross = int(np.abs(mat_planar_ref.astype(np.int64) - mat_planar_from_interleaved.astype(np.int64)).max())
    if cross != 0:
        raise SystemExit(f"[gen_data] planar pack mismatch vs interleaved deinterleave: max={cross}")

    s0_ref.tofile("./output/golden_s0.bin")
    mat_planar_ref.tofile("./output/golden_mat_c_planar.bin")

    print(f"[gen_data] LUT planar even/odd stacked [{lut_even_stacked.shape}]")
    print(f"[gen_data] src [{K_POLYS},{N}] mixPass={mix_pass}")
    print(f"[gen_data] mat_c_planar golden {mat_planar_ref.shape} cross_check_vs_interleaved={cross}")
    print("[gen_data] row layout per poly p: C_lo hh,lh,hl,ll then C_hi hh,lh,hl,ll")
