#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""verify_result — 对拍 Â / ŝ / ê / src / prf_out 与 host oracle（max=0）。"""
from __future__ import annotations

import os
import sys

import numpy as np

K = 4
N = 256
AHAT = K * K
SRC_ROWS = 2 * K
PRF_ROWS = 8
PRF_BYTES = 128


def _check_i32(name: str, got_path: str, golden_path: str, count: int) -> None:
    if not os.path.isfile(got_path) or not os.path.isfile(golden_path):
        print(f"[verify] missing {name}", file=sys.stderr)
        sys.exit(1)
    got = np.fromfile(got_path, dtype=np.int32, count=count)
    golden = np.fromfile(golden_path, dtype=np.int32, count=count)
    if got.shape[0] != count or golden.shape[0] != count:
        print(f"[verify] {name} size mismatch got={got.shape} golden={golden.shape}", file=sys.stderr)
        sys.exit(1)
    diff = np.abs(got.astype(np.int64) - golden.astype(np.int64))
    mx = int(diff.max()) if diff.size else 0
    nz = int(np.count_nonzero(diff))
    if mx != 0:
        idx = int(np.argmax(diff))
        print(
            f"[verify] FAIL {name} max={mx} nz={nz} first@{idx} "
            f"got={got[idx]} golden={golden[idx]}",
            file=sys.stderr,
        )
        sys.exit(1)
    print(f"[verify] PASS {name} max=0 ({count})")


def _check_u8(name: str, got_path: str, golden_path: str, count: int) -> None:
    if not os.path.isfile(got_path) or not os.path.isfile(golden_path):
        print(f"[verify] missing {name}", file=sys.stderr)
        sys.exit(1)
    got = np.fromfile(got_path, dtype=np.uint8, count=count)
    golden = np.fromfile(golden_path, dtype=np.uint8, count=count)
    if got.shape[0] != count or golden.shape[0] != count:
        print(f"[verify] {name} size mismatch", file=sys.stderr)
        sys.exit(1)
    diff = np.abs(got.astype(np.int16) - golden.astype(np.int16))
    mx = int(diff.max()) if diff.size else 0
    if mx != 0:
        idx = int(np.argmax(diff))
        print(f"[verify] FAIL {name} max={mx} first@{idx}", file=sys.stderr)
        sys.exit(1)
    print(f"[verify] PASS {name} max=0 ({count} B)")


def main() -> None:
    case = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    out = os.path.join(case, "output")
    _check_i32("a_hat", os.path.join(out, "a_hat.bin"), os.path.join(out, "golden_a_hat.bin"), AHAT * N)
    _check_i32("s_hat", os.path.join(out, "s_hat.bin"), os.path.join(out, "golden_s_hat.bin"), K * N)
    _check_i32("e", os.path.join(out, "e.bin"), os.path.join(out, "golden_e.bin"), K * N)
    _check_i32("src", os.path.join(out, "src.bin"), os.path.join(out, "golden_src.bin"), SRC_ROWS * N)
    _check_u8(
        "prf_out",
        os.path.join(out, "prf_out.bin"),
        os.path.join(out, "golden_prf_out.bin"),
        PRF_ROWS * PRF_BYTES,
    )
    print(f"[verify] PASS all (Â/ŝ/ê/src/prf, k={K})")


if __name__ == "__main__":
    main()
