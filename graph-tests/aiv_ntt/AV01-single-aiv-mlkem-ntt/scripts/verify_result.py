#!/usr/bin/env python3
# coding=utf-8
"""AV01：output/dst.bin ↔ output/golden.bin 逐 int32 对拍。"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

CASE = Path(__file__).resolve().parents[1]


def main() -> int:
    dst = np.fromfile(CASE / "output" / "dst.bin", dtype=np.int32)
    golden = np.fromfile(CASE / "output" / "golden.bin", dtype=np.int32)
    if dst.shape != golden.shape:
        print(f"[FAIL] shape dst={dst.shape} golden={golden.shape}")
        return 1
    if dst.shape != (256,):
        print(f"[FAIL] expected 256 coeffs, got {dst.shape}")
        return 1
    mism = np.where(dst != golden)[0]
    if len(mism):
        i = int(mism[0])
        print(f"[FAIL] mismatch at {i}: dst={dst[i]} golden={golden[i]} count={len(mism)}")
        return 1
    print("[PASS] AV01 dst ≡ golden (256 int32)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
