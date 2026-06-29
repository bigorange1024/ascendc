#!/usr/bin/env python3
# coding=utf-8
"""mixPass=6 对拍：device MAT_C_TMP_LO_EVEN vs numpy golden。"""
from __future__ import annotations

import sys

import numpy as np

SANITY_M = 16
HALF_N = 128


def check(label: str, got: np.ndarray, ref: np.ndarray) -> int:
    diff = np.abs(got.astype(np.int64) - ref.astype(np.int64))
    mx = int(diff.max())
    if mx != 0:
        flat = int(diff.argmax())
        print(f"[FAIL] {label} max_abs_diff={mx} flat_idx={flat}")
        print(f"  expected={int(ref.reshape(-1)[flat])} actual={int(got.reshape(-1)[flat])}")
        r, c = divmod(flat, HALF_N)
        print(f"  at row={r} col={c}")
        return 1
    print(f"[SUCCESS] {label} max_abs_diff=0")
    return 0


def main() -> int:
    rc = 0
    got_path = "./output/mat_c_tmp_lo_even.bin"
    gold_path = "./output/golden_mat_c_tmp_lo_even.bin"
    for p in (got_path, gold_path):
        if not __import__("os").path.isfile(p):
            print(f"[FAIL] missing {p}")
            return 1
    got = np.fromfile(got_path, dtype=np.int32).reshape(SANITY_M, HALF_N)
    gold = np.fromfile(gold_path, dtype=np.int32).reshape(SANITY_M, HALF_N)
    rc |= check("AicMmad C_lo_even (sanity)", got, gold)
    if rc == 0:
        print("[verify] MMAD sanity PASS")
    return rc


if __name__ == "__main__":
    sys.exit(main())
