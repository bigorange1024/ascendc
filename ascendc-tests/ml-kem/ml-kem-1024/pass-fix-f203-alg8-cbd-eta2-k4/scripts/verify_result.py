#!/usr/bin/env python3
# coding=utf-8
"""Alg.8 CBD η=2 探针对拍：output/src.bin vs golden_src.bin。

本文件在流水线中的位置：pass-fix-f203-alg8-cbd-eta2-k4 探针的最终验收脚本，
由 run.sh 在设备核跑完之后调用。读取 main.cpp 落盘的 output/src.bin（设备计算
结果）与 gen_data.py 落盘的 output/golden_src.bin（Python golden），逐元素
比对是否完全一致（本探针为整数域精确计算，理论上应 max_abs_diff=0）。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
ROWS = 8              # 8 行：ŝ(4) + ê(4)
N = 256                # 每行系数个数
SRC_BYTES = ROWS * N * 4  # int32 每元素 4 字节，总字节数 8*256*4=8192


def read_or_fail(path: Path) -> np.ndarray:
    """读取一个 [ROWS,N] int32 二进制文件，尺寸不符或文件缺失直接终止。"""
    if not path.is_file():
        raise SystemExit(f"missing {path}")
    data = path.read_bytes()
    if len(data) != SRC_BYTES:
        raise SystemExit(f"{path}: size {len(data)} != {SRC_BYTES}")
    return np.frombuffer(data, dtype=np.int32).reshape(ROWS, N)


def main() -> None:
    """验收主流程：读取设备输出与 golden，逐元素比对，不一致时报告首个最大差异位置。"""
    out = ROOT / "output"
    golden = read_or_fail(out / "golden_src.bin")
    actual = read_or_fail(out / "src.bin")
    if not np.array_equal(actual, golden):
        # 转 int64 避免整数环绕，取绝对差最大处的 (row, coeff) 索引用于报错定位
        diff = np.abs(actual.astype(np.int64) - golden.astype(np.int64))
        idx = int(np.unravel_index(int(np.argmax(diff)), diff.shape))
        raise SystemExit(
            f"src mismatch row={idx[0]} coeff={idx[1]}: "
            f"got {actual[idx]} golden {golden[idx]} max_abs={int(diff.max())}"
        )
    print(f"[verify] src[8,256] PASS ({SRC_BYTES} bytes exact match)")


if __name__ == "__main__":
    main()
