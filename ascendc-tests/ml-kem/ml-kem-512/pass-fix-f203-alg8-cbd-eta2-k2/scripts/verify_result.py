#!/usr/bin/env python3
# coding=utf-8
"""Alg.8 CBD η=2 探针对拍：output/src.bin vs golden_src.bin（ML-KEM-512，ROWS=4）。

本文件是 `run.sh` 的最终验收脚本。它读取设备/CPU 孪生计算结果
`output/src.bin` 与 `scripts/gen_data.py` 生成的 `output/golden_src.bin`，
按 [4,256] int32 精确比较。本探针是整数域确定性计算，PASS 条件为逐元素完全相等。
"""
from __future__ import annotations

from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
ROWS = 4
N = 256
SRC_BYTES = ROWS * N * 4


def read_or_fail(path: Path) -> np.ndarray:
    """读取一个 [ROWS,N] int32 文件；缺文件或尺寸错误直接终止，避免误报 PASS。"""
    if not path.is_file():
        raise SystemExit(f"missing {path}")
    data = path.read_bytes()
    if len(data) != SRC_BYTES:
        raise SystemExit(f"{path}: size {len(data)} != {SRC_BYTES}")
    return np.frombuffer(data, dtype=np.int32).reshape(ROWS, N)


def main() -> None:
    """读取 actual/golden 并精确对拍；失败时报告首个最大差异位置。"""
    out = ROOT / "output"
    golden = read_or_fail(out / "golden_src.bin")
    actual = read_or_fail(out / "src.bin")
    if not np.array_equal(actual, golden):
        diff = np.abs(actual.astype(np.int64) - golden.astype(np.int64))
        row, coeff = np.unravel_index(int(np.argmax(diff)), diff.shape)
        raise SystemExit(
            f"src mismatch row={int(row)} coeff={int(coeff)}: "
            f"got {int(actual[row, coeff])} golden {int(golden[row, coeff])} max_abs={int(diff.max())}"
        )
    print(f"[verify] src[4,256] PASS ({SRC_BYTES} bytes exact match)")


if __name__ == "__main__":
    main()
