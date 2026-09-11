#!/usr/bin/env python3
"""verify_result — 对拍 ŝ/u/v 与 golden（整缓冲 max=0）。"""
from __future__ import annotations

import os
import sys

import numpy as np

K = 4
N = 256


def _check(name: str, got_path: str, golden_path: str, count: int) -> None:
    if not os.path.isfile(got_path) or not os.path.isfile(golden_path):
        print(f"[verify] missing {name}", file=sys.stderr)
        sys.exit(1)
    got = np.fromfile(got_path, dtype=np.int32, count=count)
    golden = np.fromfile(golden_path, dtype=np.int32, count=count)
    if got.shape[0] != count or golden.shape[0] != count:
        print(f"[verify] {name} size mismatch", file=sys.stderr)
        sys.exit(1)
    diff = np.abs(got.astype(np.int64) - golden.astype(np.int64))
    mx = int(diff.max())
    nz = int(np.count_nonzero(diff))
    if mx != 0:
        idx = int(np.argmax(diff))
        print(
            f"[verify] FAIL {name} max={mx} nz={nz} first@{idx} "
            f"got={got[idx]} golden={golden[idx]}",
            file=sys.stderr,
        )
        sys.exit(1)
    print(f"[verify] PASS {name} max=0 ({count} coeffs)")


def main() -> None:
    case = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    out = os.path.join(case, "output")
    _check("s_hat", os.path.join(out, "s_hat.bin"), os.path.join(out, "golden_s_hat.bin"), K * N)
    _check("u", os.path.join(out, "u.bin"), os.path.join(out, "golden_u.bin"), K * N)
    _check("v", os.path.join(out, "v.bin"), os.path.join(out, "golden_v.bin"), N)
    print(f"[verify] PASS all (ŝ/u/v, k={K})")


if __name__ == "__main__":
    main()
