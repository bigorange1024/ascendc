#!/usr/bin/env python3
# coding=utf-8
"""对拍 output/a_hat.bin、re.bin 与 golden。"""
from __future__ import annotations

from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
KYBER_K = 4
KYBER_N = 256
AHAT_COEFFS = KYBER_K * KYBER_K * KYBER_N
RE_POLYS = 2 * KYBER_K + 1
RE_COEFFS = RE_POLYS * KYBER_N


def cmp_bin(name: str, out_path: Path, golden_path: Path, expected: int) -> None:
    ga = np.fromfile(golden_path, dtype=np.int32)
    got = np.fromfile(out_path, dtype=np.int32)
    if ga.size != expected:
        raise SystemExit(f"{name} golden size {ga.size}")
    if got.size != expected:
        raise SystemExit(f"{name} output size {got.size}")
    diff = int(np.max(np.abs(got.astype(np.int64) - ga.astype(np.int64))))
    if diff != 0:
        idx = int(np.argmax(got != ga))
        raise SystemExit(f"{name} max_abs_diff={diff} first_mismatch@{idx}: {got[idx]} vs {ga[idx]}")
    print(f"[verify] {name} PASS max_abs_diff=0 ({expected} coeffs)")


def main() -> None:
    out = ROOT / "output"
    cmp_bin("a_hat", out / "a_hat.bin", out / "golden_a_hat.bin", AHAT_COEFFS)
    cmp_bin("re", out / "re.bin", out / "golden_re.bin", RE_COEFFS)


if __name__ == "__main__":
    main()
