#!/usr/bin/env python3
"""gen_data — 8-poly 紧凑三段式 NTT/INTT golden（与设备/ntt_study Tag5T 同构）。"""
import os
import re
import struct
import subprocess
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_NTT_LUT_HDR = os.path.normpath(
    os.path.join(_CASE_DIR, "thirdparty/ntt_study/include/mlkem/stable/transpose_mlkem_luts_i8.h")
)
_MLKEM_REF = _SCRIPT_DIR
_NTT_LUT_HDR = os.path.normpath(
    os.path.join(_CASE_DIR, "thirdparty/ntt_study/include/mlkem/stable/transpose_mlkem_luts_i8.h")
)
_NTT_STUDY_GOLDEN = os.path.normpath(
    os.path.join(_CASE_DIR, "thirdparty/ntt_study/deliverables/sepolyvec8_ntt_f203")
)
sys.path.insert(0, _MLKEM_REF)
from mlkem_ref import stage31_mod  # noqa: E402

N = 256
HALF_N = N // 2
K = 8
K_PER_AIV = 4
M_ROWS = 2 * K
LIMBS = 4
MAT_C_PLANAR_ROWS = K * LIMBS * 2
LIMB_MASK = 0x3F
LIMB_BITS = 6
Q = 3329
SEED = 20260628


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
        raise SystemExit(f"[gen_data] LUT {symbol} size {len(nums)} != {expect}")
    return np.array(nums, dtype=np.int8).reshape(N, 512)


def lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def planar_row(slot: int, limb: int, half: int) -> int:
    return half * (K * LIMBS) + slot * LIMBS + limb


def encode_compact_k8(batch: np.ndarray, s0: np.ndarray) -> None:
    k = batch.shape[0]
    for lp in range(k):
        for r in range(N):
            v = int(batch[lp, r]) % Q
            s0[lp, r] = (v >> LIMB_BITS) & LIMB_MASK
            s0[K + lp, r] = v & LIMB_MASK


def encode_k8_s0(polys: np.ndarray) -> np.ndarray:
    s0 = np.zeros((M_ROWS, N), dtype=np.int8)
    encode_compact_k8(polys, s0)
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


def pack_bank_planar_k8(
    c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, poly_base: int, k_polys: int, out: np.ndarray
) -> None:
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


def pack_mat_c_planar_k8(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd) -> np.ndarray:
    out = np.zeros((MAT_C_PLANAR_ROWS, HALF_N), dtype=np.int32)
    pack_bank_planar_k8(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, 0, K_PER_AIV, out)
    pack_bank_planar_k8(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, K_PER_AIV, K_PER_AIV, out)
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


def golden_dst_from_planar(mat_planar: np.ndarray) -> np.ndarray:
    dst = np.zeros((K, N), dtype=np.int32)
    for slot in range(K):
        dst[slot] = merge_planar_poly(mat_planar, slot)
    return dst


def try_compare_ntt_study(src: np.ndarray, golden: np.ndarray, mode: str) -> None:
    if mode != "ntt":
        return
    ref_path = os.path.join(_NTT_STUDY_GOLDEN, "golden.bin")
    in_path = os.path.join(_NTT_STUDY_GOLDEN, "input0.bin")
    if not (os.path.isfile(ref_path) and os.path.isfile(in_path)):
        print("[gen_data] ntt_study deliverable bins missing; skip cross-check")
        return
    ref_in = np.fromfile(in_path, dtype=np.int32).reshape(K, N)
    ref_golden = np.fromfile(ref_path, dtype=np.int32).reshape(K, N)
    if np.array_equal(ref_in, src):
        diff = int(np.max(np.abs(golden.astype(np.int64) - ref_golden.astype(np.int64))))
        print(f"[gen_data] ntt_study deliverable cross-check max={diff}")
    else:
        print("[gen_data] src differs from ntt_study input0.bin; using local golden only")


def main() -> None:
    mode = os.environ.get("F203_NTT_MODE", "ntt").lower()
    if mode not in ("ntt", "intt"):
        raise SystemExit("F203_NTT_MODE must be ntt or intt")
    mix_pass = int(os.environ.get("STAGE123_POLYVEC8_MIX_PASS", "3"))

    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    rng = np.random.default_rng(SEED + (0 if mode == "ntt" else 1))
    src = rng.integers(0, Q, size=(K, N), dtype=np.int32)

    lut = load_lut_t_i8(mode)
    lut_even = lut_planar_stacked(lut, True)
    lut_odd = lut_planar_stacked(lut, False)

    s0 = encode_k8_s0(src)
    c_le, c_lo, c_he, c_ho = mat_c_tmp_golden(s0, lut)
    mat_planar = pack_mat_c_planar_k8(c_le, c_lo, c_he, c_ho)
    golden_dst = golden_dst_from_planar(mat_planar)

    src.tofile(os.path.join(_CASE_DIR, "input", "src.bin"))
    lut_even.tofile(os.path.join(_CASE_DIR, "input", "lut_even_stacked.bin"))
    lut_odd.tofile(os.path.join(_CASE_DIR, "input", "lut_odd_stacked.bin"))
    golden_dst.tofile(os.path.join(_CASE_DIR, "output", "golden_dst.bin"))
    s0.tofile(os.path.join(_CASE_DIR, "output", "golden_s0.bin"))
    mat_planar.tofile(os.path.join(_CASE_DIR, "output", "golden_mat_c.bin"))

    tiling = struct.pack("<iii", N, K, mix_pass)
    with open(os.path.join(_CASE_DIR, "input", "tiling.bin"), "wb") as f:
        f.write(tiling.ljust(64, b"\x00"))

    try_compare_ntt_study(src, golden_dst, mode)
    print(f"[gen_data] K={K} mode={mode} mixPass={mix_pass}")


if __name__ == "__main__":
    main()
