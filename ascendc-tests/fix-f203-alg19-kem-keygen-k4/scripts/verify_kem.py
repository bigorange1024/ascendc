#!/usr/bin/env python3
"""verify_kem.py — output/ek_kem.bin + dk_kem.bin vs golden 字节对拍。"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "output"


def _cmp(name: str, got_p: Path, gold_p: Path) -> None:
    if not got_p.is_file():
        raise SystemExit(f"missing output/{name}")
    if not gold_p.is_file():
        raise SystemExit(f"missing output/golden_{name} (run gen_data.py first)")
    got = np.fromfile(got_p, dtype=np.uint8)
    exp = np.fromfile(gold_p, dtype=np.uint8)
    if got.shape != exp.shape:
        raise SystemExit(f"[verify] FAIL shape {name} {got.shape} vs {exp.shape}")
    diff = np.abs(got.astype(np.int16) - exp.astype(np.int16))
    mx = int(diff.max()) if diff.size else 0
    if mx != 0:
        raise SystemExit(f"[verify] FAIL {name} max={mx}")
    print(f"[verify] PASS {name} max=0 ({got.size} bytes)")


def main() -> int:
    _cmp("ek_kem.bin", OUT / "ek_kem.bin", OUT / "golden_ek_kem.bin")
    _cmp("dk_kem.bin", OUT / "dk_kem.bin", OUT / "golden_dk_kem.bin")
    print("[verify] KEM KeyGen overall PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
