#!/usr/bin/env python3
# coding=utf-8
"""Alg.8 CBD η=2 探针 golden：prf_out[8,128] → src[8,256] int32。

与 FIPS 203 SamplePolyCBD(η=2) 及 fips203_se_sample.c 同语义。
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
sys.path.insert(0, str(REPO / "library/shared/fips203_se_sample"))
from golden_se_sampling import sample_poly_cbd2  # noqa: E402

ROWS = 8
PRF_BYTES = 128
N = 256
SEED = int(os.environ.get("SEED_D", "20260619"))


def main() -> None:
    rng = np.random.default_rng(SEED)
    prf = rng.integers(0, 256, size=ROWS * PRF_BYTES, dtype=np.uint8)
    src = np.zeros((ROWS, N), dtype=np.int32)
    for row in range(ROWS):
        chunk = bytes(prf[row * PRF_BYTES : (row + 1) * PRF_BYTES])
        src[row] = sample_poly_cbd2(chunk)

    inp = ROOT / "input"
    out = ROOT / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)
    prf.tofile(inp / "prf_out.bin")
    src.tofile(out / "golden_src.bin")
    print(f"[gen_data] SEED_D={SEED} prf={ROWS}x{PRF_BYTES}B src={ROWS}x{N} int32")


if __name__ == "__main__":
    main()
