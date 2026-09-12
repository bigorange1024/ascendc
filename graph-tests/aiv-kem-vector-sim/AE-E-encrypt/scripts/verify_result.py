#!/usr/bin/env python3
# coding=utf-8
"""AE-E：output/c.bin ≡ liboqs golden（max=0）。"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

CASE = Path(__file__).resolve().parents[1]


def main() -> int:
    dst = np.fromfile(CASE / "output" / "c.bin", dtype=np.uint8)
    golden = np.fromfile(CASE / "output" / "golden_c.bin", dtype=np.uint8)
    if dst.shape != (1568,) or golden.shape != (1568,):
        print(f"[FAIL] shape dst={dst.shape} golden={golden.shape}")
        return 1
    diff = np.abs(dst.astype(np.int16) - golden.astype(np.int16))
    mx = int(diff.max())
    mism = int(np.count_nonzero(diff))
    print(f"[AE-E] c vs liboqs max={mx} mism_bytes={mism}")
    if mx != 0:
        i = int(np.argmax(diff))
        print(f"[FAIL] first@{i} dst={dst[i]} golden={golden[i]}")
        return 1
    print("[PASS] AE-E c ≡ liboqs max=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
