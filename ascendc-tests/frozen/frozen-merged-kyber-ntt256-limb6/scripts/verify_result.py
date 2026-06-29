#!/usr/bin/python3
# coding=utf-8
"""Phase D：dst[256] int32 vs golden.bin，max_abs_diff=0 为 PASS。"""
import sys

import numpy as np

N = 256


def main() -> int:
    out = np.fromfile("./output/dst.bin", dtype=np.int32, count=N)
    gold = np.fromfile("./output/golden.bin", dtype=np.int32, count=N)
    diff = np.abs(out.astype(np.int64) - gold.astype(np.int64))
    max_diff = int(diff.max())
    if max_diff != 0:
        idx = int(diff.argmax())
        print(f"max_abs_diff={max_diff} at idx={idx}")
        print(f"  expected={int(gold[idx])} actual={int(out[idx])}")
        return 1
    print(f"[SUCCESS] limb6 D baseline matches golden (max_abs_diff=0)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
