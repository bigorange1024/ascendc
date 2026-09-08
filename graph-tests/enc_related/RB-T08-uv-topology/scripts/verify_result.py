#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T08 verify：u,v 与 golden 逐元素一致（I/O 对拍）。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output"

K = 4
N = 256
U_ELEMS = K * N
V_ELEMS = N


def _cmp_i32(a: Path, b: Path, n: int, label: str) -> bool:
    if not a.is_file() or not b.is_file():
        print(f"[FAIL] missing {a.name} or {b.name}", file=sys.stderr)
        return False
    xa = np.fromfile(a, dtype=np.int32)
    xb = np.fromfile(b, dtype=np.int32)
    if xa.size != n or xb.size != n:
        print(f"[FAIL] {label} size {xa.size}/{xb.size} expect {n}", file=sys.stderr)
        return False
    if not np.array_equal(xa, xb):
        diff = int(np.max(np.abs(xa.astype(np.int64) - xb.astype(np.int64))))
        print(f"[FAIL] {label} max_abs_diff={diff}", file=sys.stderr)
        return False
    return True


def main() -> int:
    ok = True
    ok = _cmp_i32(OUT / "u.bin", OUT / "golden_u.bin", U_ELEMS, "u") and ok
    ok = _cmp_i32(OUT / "v.bin", OUT / "golden_v.bin", V_ELEMS, "v") and ok
    if not ok:
        return 1
    print("[SUCCESS] u,v match golden (Host uv topology / G3)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
