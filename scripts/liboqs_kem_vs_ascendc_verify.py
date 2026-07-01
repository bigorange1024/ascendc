#!/usr/bin/env python3
"""liboqs_kem_vs_ascendc_verify.py — liboqs fixture vs AscendC KEM KeyGen 输出对拍。"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

EK_BYTES = 1568
DK_BYTES = 3168


def _cmp(label: str, got: Path, ref: Path) -> int:
    g = np.fromfile(got, dtype=np.uint8)
    r = np.fromfile(ref, dtype=np.uint8)
    if g.shape != r.shape:
        print(f"[liboqs_kem_vs] FAIL {label} shape {g.shape} vs {r.shape}")
        return 1
    mx = int(np.max(np.abs(g.astype(np.int16) - r.astype(np.int16)))) if g.size else 0
    if mx != 0:
        print(f"[liboqs_kem_vs] FAIL {label} max={mx}")
        return 1
    print(f"[liboqs_kem_vs] PASS {label} max=0 ({g.size} bytes)")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--fixture-dir", type=Path, required=True)
    ap.add_argument("--ascendc-out", type=Path, required=True)
    args = ap.parse_args()
    rc = 0
    rc |= _cmp("ek_kem", args.ascendc_out / "ek_kem.bin", args.fixture_dir / "ek_kem.bin")
    rc |= _cmp("dk_kem", args.ascendc_out / "dk_kem.bin", args.fixture_dir / "dk_kem.bin")
    return rc


if __name__ == "__main__":
    sys.exit(main())
