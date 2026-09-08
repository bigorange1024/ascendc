#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T07 verify：固定 ek/coins → ρ/y/e₁/e₂ 与 golden 逐字节一致。

硬条件：rho / y / e1 / e2 与 golden_* 完全一致；长度契约正确。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output"

EK_RHO = 32
Y_ELEMS = 4 * 256
E1_ELEMS = 4 * 256
E2_ELEMS = 256


def _cmp_bytes(a: Path, b: Path, label: str) -> bool:
    if not a.is_file() or not b.is_file():
        print(f"[FAIL] missing {a.name} or {b.name}", file=sys.stderr)
        return False
    ba, bb = a.read_bytes(), b.read_bytes()
    if ba != bb:
        print(f"[FAIL] {label} mismatch len={len(ba)}/{len(bb)}", file=sys.stderr)
        return False
    return True


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
    ok = _cmp_bytes(OUT / "rho.bin", OUT / "golden_rho.bin", "rho") and ok
    ok = _cmp_i32(OUT / "y.bin", OUT / "golden_y.bin", Y_ELEMS, "y") and ok
    ok = _cmp_i32(OUT / "e1.bin", OUT / "golden_e1.bin", E1_ELEMS, "e1") and ok
    ok = _cmp_i32(OUT / "e2.bin", OUT / "golden_e2.bin", E2_ELEMS, "e2") and ok
    # 尾 ρ 长度
    rho = (OUT / "rho.bin").read_bytes()
    if len(rho) != EK_RHO:
        print(f"[FAIL] rho len {len(rho)} != {EK_RHO}", file=sys.stderr)
        ok = False
    if not ok:
        return 1
    print("[SUCCESS] ρ←ek尾 + coins→(y,e1,e2) match golden (Host prep shell)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
