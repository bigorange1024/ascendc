#!/usr/bin/python3
# coding=utf-8
import sys

import numpy as np

K = 8
N = 256


def verify_result(output_path: str, golden_path: str) -> bool:
    output = np.fromfile(output_path, dtype=np.int32).reshape(K, N)
    golden = np.fromfile(golden_path, dtype=np.int32).reshape(K, N)
    if np.array_equal(output, golden):
        print("test pass (max_abs_diff=0)")
        return True
    diff = np.abs(output.astype(np.int64) - golden.astype(np.int64))
    max_diff = int(diff.max())
    idx = int(np.argmax(diff))
    poly, col = divmod(idx, N)
    print(f"max_abs_diff={max_diff} at poly={poly} coeff={col}")
    print(f"  expected={golden[poly, col]} actual={output[poly, col]}")
    return False


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("usage: verify_result.py <output.bin> <golden.bin>")
        sys.exit(1)
    ok = verify_result(sys.argv[1], sys.argv[2])
    sys.exit(0 if ok else 1)
