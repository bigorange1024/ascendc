#!/usr/bin/env python3
# coding=utf-8
"""AE-P：设备产出 c 与 K ≡ liboqs（强完成：K 来自设备 FO）。"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

CASE = Path(__file__).resolve().parents[1]


def main() -> int:
    dst = np.fromfile(CASE / "output" / "c.bin", dtype=np.uint8)
    golden = np.fromfile(CASE / "output" / "golden_c.bin", dtype=np.uint8)
    k = np.fromfile(CASE / "output" / "K.bin", dtype=np.uint8)
    kg = np.fromfile(CASE / "output" / "golden_K.bin", dtype=np.uint8)
    rc = 0
    if dst.shape != (1568,) or golden.shape != (1568,):
        print(f"[FAIL] c shape dst={dst.shape} golden={golden.shape}")
        rc = 1
    else:
        diff = np.abs(dst.astype(np.int16) - golden.astype(np.int16))
        mx = int(diff.max())
        print(f"[AE-P] c vs liboqs max={mx} mism_bytes={int(np.count_nonzero(diff))}")
        if mx != 0:
            rc = 1
    if k.shape != (32,) or kg.shape != (32,):
        print(f"[FAIL] K shape {k.shape} vs {kg.shape}（设备未写出？）")
        rc = 1
    else:
        diffk = np.abs(k.astype(np.int16) - kg.astype(np.int16))
        mxk = int(diffk.max())
        print(f"[AE-P] K vs liboqs max={mxk} (DEVICE_FO)")
        if mxk != 0:
            rc = 1
    if rc == 0:
        print("[PASS] AE-P c/K ≡ liboqs (strong: DEVICE_FO + Encrypt)")
    return rc


if __name__ == "__main__":
    sys.exit(main())
