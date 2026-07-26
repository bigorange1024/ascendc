#!/usr/bin/env python3
"""verify_result — 对拍 output/poly.bin 与 golden_poly.bin。"""
import os
import sys

import numpy as np

N = 256


def main() -> None:
    case = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    out_path = os.path.join(case, "output", "poly.bin")
    golden_path = os.path.join(case, "output", "golden_poly.bin")
    if not os.path.isfile(out_path):
        print(f"[verify] missing {out_path}", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(golden_path):
        print(f"[verify] missing {golden_path}", file=sys.stderr)
        sys.exit(1)

    got = np.fromfile(out_path, dtype=np.int32, count=N)
    golden = np.fromfile(golden_path, dtype=np.int32, count=N)
    if got.shape[0] != N or golden.shape[0] != N:
        print("[verify] size mismatch", file=sys.stderr)
        sys.exit(1)

    diff = np.abs(got.astype(np.int64) - golden.astype(np.int64))
    mx = int(diff.max())
    if mx != 0:
        idx = int(np.argmax(diff))
        print(f"[verify] FAIL max={mx} first@{idx} got={got[idx]} golden={golden[idx]}", file=sys.stderr)
        sys.exit(1)
    print(f"[verify] PASS max=0 ({N} coeffs)")


if __name__ == "__main__":
    main()
