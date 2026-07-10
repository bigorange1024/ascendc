#!/usr/bin/env python3
"""
verify_result.py — 对比设备输出与 gen_data 生成的 golden。

本文件在流水线中的位置：本探针的最终验收脚本，由 run.sh 在设备核跑完之后调用。
读取 main.cpp 落盘的 output/{out,s0,mat_c}.bin（设备各阶段中间/最终结果）与
gen_data.py 落盘的 output/golden_{out,s0,mat_c}.bin（Python golden）逐元素比对。

按 mixPass 只校验本阶段会产生/更新的缓冲区：
  mixPass 0：out、s0、mat_c 全比
  mixPass 1：仅 s0（S1）
  mixPass 2：s0（preset 或前次跑）、mat_c（S2）
  mixPass 3：mat_c（preset）、out（S3+encode）

通过条件：各对比项 max_abs_diff == 0。
"""
import os
import sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "output")

K_S0 = 64 * 64


def max_abs_diff(a: np.ndarray, b: np.ndarray) -> int:
    """转 int32 计算两数组逐元素绝对差的最大值，避免 int8/uint8 减法环绕。"""
    return int(np.max(np.abs(a.astype(np.int32) - b.astype(np.int32))))


def main() -> int:
    """验收主流程：按 mixPass 选择需要比对的缓冲区，逐项检查 max_abs_diff==0。"""
    mix_pass = int(os.environ.get("TOY_MIX_PASS", os.environ.get("PLANAR_MIX_PASS", "0")))

    if mix_pass in (0, 3):
        # mixPass=0（全流程）或 3（仅 S3+encode）：out 是本次实际经过设备计算的最终产物
        out = np.fromfile(os.path.join(OUT_DIR, "out.bin"), dtype=np.int8, count=K_S0)
        golden = np.fromfile(os.path.join(OUT_DIR, "golden_out.bin"), dtype=np.int8, count=K_S0)
        d = max_abs_diff(out, golden)
        print(f"out vs golden_out: max_abs_diff={d}")
        if d != 0:
            return 1

    if mix_pass in (0, 1, 2):
        # mixPass=0/1（含 S1）或 2（S1 被跳过但由 preset 文件回填）：校验 s0（左矩阵 A）
        s0 = np.fromfile(os.path.join(OUT_DIR, "s0.bin"), dtype=np.int8, count=K_S0)
        golden_s0 = np.fromfile(os.path.join(OUT_DIR, "golden_s0.bin"), dtype=np.int8, count=K_S0)
        d = max_abs_diff(s0, golden_s0)
        print(f"s0 vs golden_s0: max_abs_diff={d}")
        if d != 0:
            return 1

    if mix_pass in (0, 2, 3):
        # mixPass=0/2（含 S2）或 3（S2 被跳过但由 preset 文件回填）：校验 mat_c（Cube 输出）
        mat_c = np.fromfile(os.path.join(OUT_DIR, "mat_c.bin"), dtype=np.int32, count=K_S0)
        golden_c = np.fromfile(os.path.join(OUT_DIR, "golden_mat_c.bin"), dtype=np.int32, count=K_S0)
        d = max_abs_diff(mat_c, golden_c)
        print(f"mat_c vs golden_mat_c: max_abs_diff={d}")
        if d != 0:
            return 1

    print("[verify] PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
