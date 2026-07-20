#!/usr/bin/env python3
"""
gate_g2.py — G2 golden：r polyvec [4,256] → r̂ NTT（Tag5T Stage1–3，与 stage123 gen_data 同构）。

自包含：解析 vendored transpose_mlkem_luts_i8.h；禁止 liboqs。
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

import numpy as np

K = 4
N = 256
HALF_N = N // 2
LIMBS = 4
MAT_C_PLANAR_ROWS = K * LIMBS * 2
LIMB_BITS = 6
Q = 3329

CASE = Path(__file__).resolve().parent.parent.parent
LUT_HDR = CASE / "compute/ntt_r/thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h"


def load_lut_t_i8() -> np.ndarray:
    txt = LUT_HDR.read_text(encoding="utf-8")
    i0 = txt.index("kMlkemLimb6Ntt_T_i8")
    i1 = txt.index("{", i0)
    i2 = txt.index("};", i1)
    nums = [int(x) for x in re.findall(r"-?\d+", txt[i1 + 1 : i2])]
    expect = N * 512
    if len(nums) != expect:
        raise SystemExit(f"[gate_g2] LUT size {len(nums)} != {expect}")
    return np.array(nums, dtype=np.int8).reshape(N, 512)


def planar_row(slot: int, limb: int, half: int) -> int:
    return half * (K * LIMBS) + slot * LIMBS + limb


def encode_compact(polys: np.ndarray) -> np.ndarray:
    """[K,N] int32 → S0 [2K,N] int8 hi/lo。"""
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
    pack_bank(c_le, c_lo, c_he, c_ho, 0, K // 2, out)
    pack_bank(c_le, c_lo, c_he, c_ho, K // 2, K // 2, out)
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


def ntt_r_hat_golden(r: np.ndarray) -> np.ndarray:
    if r.shape != (K, N):
        raise SystemExit(f"[gate_g2] r shape {r.shape} != ({K},{N})")
    lut = load_lut_t_i8()
    s0 = encode_compact(r)
    c_le, c_lo, c_he, c_ho = mat_c_tmp_golden(s0, lut)
    mat_planar = pack_mat_c_planar(c_le, c_lo, c_he, c_ho)
    dst = np.zeros((K, N), dtype=np.int32)
    for slot in range(K):
        dst[slot] = merge_planar_poly(mat_planar, slot)
    return dst


def main() -> None:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <case_dir> <out_dir>", file=sys.stderr)
        sys.exit(1)
    case_dir = Path(sys.argv[1])
    out_dir = Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)

    r_path = out_dir / "r.bin"
    if not r_path.is_file():
        raise SystemExit(f"[gate_g2] missing {r_path} (run G1 first)")
    r = np.fromfile(r_path, dtype=np.int32).reshape(K, N)
    r_hat = ntt_r_hat_golden(r)
    r_hat.astype(np.int32).tofile(out_dir / "golden_r_hat.bin")
    print(f"[gate_g2] r_hat golden OK ({r_hat.nbytes}B)")


if __name__ == "__main__":
    main()
