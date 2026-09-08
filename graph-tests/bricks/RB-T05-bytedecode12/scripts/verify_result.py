#!/usr/bin/env python3
"""verify_result — 对拍 output/t_hat.bin 与 golden_t_hat.bin（k×256 int32）。"""
from __future__ import annotations

import os
import sys

import numpy as np

K = 4
N = 256
TOTAL = K * N


def main() -> None:
    case = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    out_path = os.path.join(case, "output", "t_hat.bin")
    golden_path = os.path.join(case, "output", "golden_t_hat.bin")
    if not os.path.isfile(out_path) or not os.path.isfile(golden_path):
        print("[verify] missing output or golden", file=sys.stderr)
        sys.exit(1)

    got = np.fromfile(out_path, dtype=np.int32, count=TOTAL)
    golden = np.fromfile(golden_path, dtype=np.int32, count=TOTAL)
    if got.shape[0] != TOTAL or golden.shape[0] != TOTAL:
        print("[verify] size mismatch", file=sys.stderr)
        sys.exit(1)

    diff = np.abs(got.astype(np.int64) - golden.astype(np.int64))
    mx = int(diff.max())
    nz = int(np.count_nonzero(diff))
    if mx != 0:
        idx = int(np.argmax(diff))
        print(
            f"[verify] FAIL max={mx} nz={nz} first@{idx} got={got[idx]} golden={golden[idx]}",
            file=sys.stderr,
        )
        sys.exit(1)
    print(f"[verify] PASS max=0 ({TOTAL} coeffs, k={K})")


if __name__ == "__main__":
    main()
