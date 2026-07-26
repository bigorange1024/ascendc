#!/usr/bin/env python3
# coding=utf-8
"""
@file verify_result.py
@brief Alg.11/12 内积探针：对拍 output/t_hat.bin 与 golden_t_hat.bin。

## 流水线位置
pass-fix-f203-alg11-12-innerproduct-k4（或 halfrows 变体）设备输出 t_hat
与 host 参考逐系数 max_abs_diff=0。

## I/O
- 输入：`output/golden_t_hat.bin`、`output/t_hat.bin`
- 形状：[P_OUT, N] int32（见 ip_shape.py）

## 与设备
仅黑盒 oracle 对拍；禁止把本脚本当作 AscendC 实现规格。
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
