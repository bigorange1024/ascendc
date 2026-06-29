#!/usr/bin/env python3
# coding=utf-8
"""Stage1+2 对拍：S0、mat_c_planar vs golden。"""
import os
import sys

import numpy as np

K_POLYS = 8
N = 256
HALF_N = N // 2
M_ROWS = 2 * K_POLYS
MAT_C_PLANAR_ROWS = K_POLYS * 4 * 2


def check(label: str, got: np.ndarray, ref: np.ndarray) -> int:
    diff = np.abs(got.astype(np.int64) - ref.astype(np.int64))
    mx = int(diff.max())
    if mx != 0:
        flat = int(diff.argmax())
        print(f"[FAIL] {label} max_abs_diff={mx} flat_idx={flat}")
        print(f"  expected={int(ref.reshape(-1)[flat])} actual={int(got.reshape(-1)[flat])}")
        return 1
    print(f"[SUCCESS] {label} max_abs_diff=0")
    return 0


def main() -> int:
    rc = 0
    if os.path.isfile("./output/golden_s0.bin") and os.path.isfile("./output/s0.bin"):
        g = np.fromfile("./output/golden_s0.bin", dtype=np.int8).reshape(M_ROWS, N)
        s = np.fromfile("./output/s0.bin", dtype=np.int8).reshape(M_ROWS, N)
        rc |= check("S0 poly-batch vs golden_s0", s, g)
    else:
        print("[skip] s0 golden/output missing")

    if os.path.isfile("./output/golden_mat_c_planar.bin") and os.path.isfile("./output/mat_c_planar.bin"):
        g = np.fromfile("./output/golden_mat_c_planar.bin", dtype=np.int32).reshape(MAT_C_PLANAR_ROWS, HALF_N)
        c = np.fromfile("./output/mat_c_planar.bin", dtype=np.int32).reshape(MAT_C_PLANAR_ROWS, HALF_N)
        rc |= check("mat_c_planar vs golden", c, g)
    else:
        print("[FAIL] mat_c_planar golden/output missing")
        rc = 1

    return rc


if __name__ == "__main__":
    sys.exit(main())
