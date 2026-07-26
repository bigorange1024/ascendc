#!/usr/bin/env python3
# coding=utf-8
"""
【文件头】MultiplyNTTs 探针输出对拍脚本。

本文件在流水线中的位置：run.sh 在 kernel 写出 output/h.bin 后调用。
作用：将 h.bin 与 gen_data 生成的 golden_h.bin 逐 int32 比较。
与 golden 关系：仅验 I/O 等价；不检查设备实现是否与参考源码同构。
"""
import os
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))

N = 256


def main() -> None:
    """
    对拍 output/golden_h.bin 与 output/h.bin。
    前置：两文件均存在且各含 N 个 int32。
    成功：打印 [OK]；失败：打印前 8 处差异并 exit 1。
    """
    golden_path = os.path.join(_CASE_DIR, "output", "golden_h.bin")
    out_path = os.path.join(_CASE_DIR, "output", "h.bin")
    # 缺文件则无法对拍
    for p in (golden_path, out_path):
        if not os.path.isfile(p):
            print(f"[ERROR] missing {p}")
            sys.exit(1)

    golden = np.fromfile(golden_path, dtype=np.int32)
    actual = np.fromfile(out_path, dtype=np.int32)
    if golden.shape != (N,) or actual.shape != (N,):
        print(f"[ERROR] shape golden={golden.shape} actual={actual.shape}")
        sys.exit(1)

    if np.array_equal(golden, actual):
        print(f"[OK] verify: all {N} coeffs match golden")
        return

    # 打印首批差异便于定位布局/约化错误
    diff = np.where(golden != actual)[0]
    print(f"[ERROR] {diff.size} mismatches, first 8:")
    for i in diff[:8]:
        print(f"  i={i} golden={golden[i]} actual={actual[i]}")
    sys.exit(1)


if __name__ == "__main__":
    main()
