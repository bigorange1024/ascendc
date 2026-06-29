#!/usr/bin/env python3
# coding=utf-8
"""Alg.8 CBD η=2 探针对拍：output/src.bin vs golden_src.bin。"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
ROWS = 8
N = 256
SRC_BYTES = ROWS * N * 4


def read_or_fail(path: Path) -> np.ndarray:
    if not path.is_file():
        raise SystemExit(f"missing {path}")
    data = path.read_bytes()
    if len(data) != SRC_BYTES:
        raise SystemExit(f"{path}: size {len(data)} != {SRC_BYTES}")
    return np.frombuffer(data, dtype=np.int32).reshape(ROWS, N)


def main() -> None:
    out = ROOT / "output"
    golden = read_or_fail(out / "golden_src.bin")
    actual = read_or_fail(out / "src.bin")
    if not np.array_equal(actual, golden):
        diff = np.abs(actual.astype(np.int64) - golden.astype(np.int64))
        idx = int(np.unravel_index(int(np.argmax(diff)), diff.shape))
        raise SystemExit(
            f"src mismatch row={idx[0]} coeff={idx[1]}: "
            f"got {actual[idx]} golden {golden[idx]} max_abs={int(diff.max())}"
        )
    print(f"[verify] src[8,256] PASS ({SRC_BYTES} bytes exact match)")


if __name__ == "__main__":
    main()
