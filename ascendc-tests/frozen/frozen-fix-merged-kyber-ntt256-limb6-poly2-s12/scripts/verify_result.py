#!/usr/bin/python3
# coding=utf-8
import sys

import numpy as np

K = 4
N = 256


def main():
    out = np.fromfile("./output/dst.bin", dtype=np.int32).reshape(K, N)
    gold = np.fromfile("./output/golden.bin", dtype=np.int32).reshape(K, N)
    diff = np.abs(out.astype(np.int64) - gold.astype(np.int64))
    max_diff = int(diff.max())
    if max_diff == 0:
        print(f"[SUCCESS] poly2 s12 matches golden (max_abs_diff=0)")
        return 0
    row, col = np.unravel_index(int(diff.argmax()), diff.shape)
    print(f"max_abs_diff={max_diff} at row={row} col={col}")
    print(f"  expected={int(gold[row, col])} actual={int(out[row, col])}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
