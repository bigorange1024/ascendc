#!/usr/bin/env python3
# coding=utf-8
"""
@file verify_result.py
@brief Alg.11/12 内积探针（k2 1+1 分片）：对拍 t_hat 与 golden。

## 流水线位置
k2 变体由两个 AIV 分别写一行；形状为 [P_OUT, N]。

## 与设备
仅 I/O 对拍；非 AscendC 规格。
"""
import os
import sys

import numpy as np

from ip_shape import N, P_OUT

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))


def main() -> None:
    """读 golden 与实际输出，reshape 后比较 max_abs_diff；非零则 exit 1。"""
    golden = np.fromfile(os.path.join(_CASE_DIR, "output", "golden_t_hat.bin"), dtype=np.int32)
    actual = np.fromfile(os.path.join(_CASE_DIR, "output", "t_hat.bin"), dtype=np.int32)
    golden = golden.reshape(P_OUT, N)
    actual = actual.reshape(P_OUT, N)

    if golden.shape != actual.shape:
        print(f"[ERROR] shape golden={golden.shape} actual={actual.shape}")
        sys.exit(1)

    diff = np.abs(golden.astype(np.int64) - actual.astype(np.int64))
    max_diff = int(diff.max())
    if max_diff != 0:
        idx = int(np.argmax(diff))
        p, c = divmod(idx, N)
        print(f"[ERROR] max_abs_diff={max_diff} at p={p} c={c} golden={golden.flat[idx]} actual={actual.flat[idx]}")
        sys.exit(1)

    print(f"[OK] verify: t_hat[{P_OUT},{N}] max_abs_diff=0")


if __name__ == "__main__":
    main()
