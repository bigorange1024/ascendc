#!/usr/bin/env python3
# coding=utf-8
"""对拍 output/a_hat.bin 与 golden_a_hat.bin（16×256 int32）。"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
KYBER_K = 4
KYBER_N = 256
AHAT_BYTES = KYBER_K * KYBER_K * KYBER_N * 4


def main() -> None:
  out = ROOT / "output"
  ga = np.fromfile(out / "golden_a_hat.bin", dtype=np.int32)
  a = np.fromfile(out / "a_hat.bin", dtype=np.int32)
  if ga.size != KYBER_K * KYBER_K * KYBER_N:
    raise SystemExit(f"golden size {ga.size}")
  if a.size != ga.size:
    raise SystemExit(f"a_hat size {a.size} != golden {ga.size}")
  diff = int(np.max(np.abs(a.astype(np.int64) - ga.astype(np.int64))))
  if diff != 0:
    idx = int(np.argmax(a != ga))
    raise SystemExit(f"a_hat max_abs_diff={diff} first_mismatch@{idx}: {a[idx]} vs {ga[idx]}")
  print("[verify] a_hat PASS max_abs_diff=0 (16 polys)")


if __name__ == "__main__":
  main()
