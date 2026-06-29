#!/usr/bin/env python3
# coding=utf-8
import os
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))

N = 256


def main() -> None:
    golden_path = os.path.join(_CASE_DIR, "output", "golden_h.bin")
    out_path = os.path.join(_CASE_DIR, "output", "h.bin")
    for p in (golden_path, out_path):
        if not os.path.isfile(p):
            print(f"[ERROR] missing {p}")
            sys.exit(1)

    golden = np.fromfile(golden_path, dtype=np.int32)
    actual = np.fromfile(out_path, dtype=np.int32)
    if golden.shape != (N,) or actual.shape != (N,):
        print(f"[ERROR] shape golden={golden.shape} actual={actual.shape}")
        sys.exit(1)

    if np.array_equal(golden, actual):
        print(f"[OK] verify: all {N} coeffs match golden")
        return

    diff = np.where(golden != actual)[0]
    print(f"[ERROR] {diff.size} mismatches, first 8:")
    for i in diff[:8]:
        print(f"  i={i} golden={golden[i]} actual={actual[i]}")
    sys.exit(1)


if __name__ == "__main__":
    main()
