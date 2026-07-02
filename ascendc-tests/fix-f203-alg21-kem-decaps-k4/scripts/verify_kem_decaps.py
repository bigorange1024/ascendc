#!/usr/bin/env python3
"""verify_kem_decaps.py — 对拍 output/K.bin vs golden_K（同 SEED_D 下 Encaps 的 K）。"""
from __future__ import annotations

import sys
from pathlib import Path


def max_diff(a: bytes, b: bytes) -> int:
    n = min(len(a), len(b))
    m = 0
    for i in range(n):
        m = max(m, abs(a[i] - b[i]))
    if len(a) != len(b):
        return max(m, 256)
    return m


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    out = root / "output"
    k = (out / "K.bin").read_bytes()
    gk = (out / "golden_K.bin").read_bytes()
    mk = max_diff(k, gk)
    print(f"[verify_kem_decaps] K max={mk}")
    if mk != 0:
        sys.exit(1)
    print("[verify_kem_decaps] PASS")


if __name__ == "__main__":
    main()
