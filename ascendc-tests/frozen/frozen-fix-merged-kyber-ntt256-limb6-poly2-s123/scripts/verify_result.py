#!/usr/bin/python3
# coding=utf-8
import os
import sys

import numpy as np

K_POLYS = 2
N = 256
_LIMB6_GOLDEN = os.path.normpath(
    os.path.join(os.path.dirname(__file__), "../../frozen/frozen-merged-kyber-ntt256-limb6/output/golden.bin")
)


def main():
    out = np.fromfile("./output/dst.bin", dtype=np.int32).reshape(K_POLYS, N)
    gold = np.fromfile("./output/golden.bin", dtype=np.int32).reshape(K_POLYS, N)
    diff = np.abs(out.astype(np.int64) - gold.astype(np.int64))
    max_diff = int(diff.max())
    if max_diff != 0:
        row, col = np.unravel_index(int(diff.argmax()), diff.shape)
        print(f"max_abs_diff={max_diff} at row={row} col={col}")
        print(f"  expected={int(gold[row, col])} actual={int(out[row, col])}")
        return 1

    print(f"[SUCCESS] poly2 s123 matches golden (max_abs_diff=0)")

    if os.path.isfile(_LIMB6_GOLDEN):
        limb6 = np.fromfile(_LIMB6_GOLDEN, dtype=np.int32).reshape(N)
        for p in range(K_POLYS):
            d = int(np.abs(out[p].astype(np.int64) - limb6.astype(np.int64)).max())
            if d != 0:
                print(f"[FAIL] poly{p} vs limb6 max_abs_diff={d}")
                return 1
        print(f"[SUCCESS] each poly matches merged-kyber-ntt256-limb6 golden")
    else:
        print(f"[skip] limb6 golden not found at {_LIMB6_GOLDEN}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
