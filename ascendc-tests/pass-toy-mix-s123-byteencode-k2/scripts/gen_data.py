#!/usr/bin/env python3
"""
gen_data.py — pass-toy-mix-s123-byteencode-k2 输入与 golden 生成。

本文件在流水线中的位置：本探针的 Host 侧数据生成脚本，由 run.sh 在设备编译/
运行前调用。生成 3 类内容：
  1. input/tiling.bin —— 64 字节 tiling 结构体，携带 mixPass 供设备核分阶段调试；
  2. input/src.bin、input/lut.bin —— 设备核实际输入（src 全 0，S1 填数规则不
     依赖其非零；lut 为单位阵 I₆₄，模拟真实右矩阵 B）；
  3. output/golden_s0.bin、golden_mat_c.bin、golden_out.bin —— 用 NumPy 一次性
     算出的三阶段 golden 中间结果与最终结果，供 verify_result.py 按 mixPass 分段对拍。
与 AscendC 实现的关系：golden 仅提供黑盒 oracle，不是设备侧 Stage1（limb+填数）/
Stage2（Cube MMAD）/Stage3（Adds+func1）实现须复刻的算法规格，只要求最终各阶段
产物与本脚本计算结果逐元素一致。

与 TOY_MIX_S123.md / 设备侧逻辑对齐：

  左矩阵 A（S0）：
    A[i] = i % 128，i 为行优先 flat 下标（设备 S1 填数规则）

  右矩阵 B（LUT）：
    B = I₆₄（int8 单位阵）

  Cube 输出 C（MAT_C）：
    C = A @ B = A（int32 扩宽累加；元素 ∈ [0,127]，无溢出）

  最终 out：
    out[i] = (C[i] + 1) % 64

环境变量 TOY_MIX_PASS（默认 0）写入 tiling.bin，供分阶段调试。
"""
import os
import struct
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IN_DIR = os.path.join(ROOT, "input")
OUT_DIR = os.path.join(ROOT, "output")

K_ROWS = 64
K_COLS = 64
K_SRC_TOTAL = 2048  # int32 输入总长；S1 每 AIV 1024
K_S0 = K_ROWS * K_COLS  # 4096


def write_tiling(mix_pass: int) -> None:
    """写入 64 字节 tiling.bin：tileLength=64（占位），mixPass 控制 kernel 阶段。

    payload 布局须与设备/Host 共用的 `TilingData` 结构体（tiling.h）字段顺序一致：
    第一个 int32 为 tileLength（本探针未使用，此处填 K_COLS=64 占位），第二个
    int32 为 mixPass；其余填零补齐到固定 64 字节（main.cpp 按 64 字节读取校验）。
    """
    payload = struct.pack("<ii", K_COLS, mix_pass)
    payload += b"\x00" * (64 - len(payload))
    with open(os.path.join(IN_DIR, "tiling.bin"), "wb") as f:
        f.write(payload)


def golden_pipeline() -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """
    一行 NumPy 算完整 golden 链，避免手算 4096 元素。

    对应设备侧三阶段：
      a_flat  —— Stage1 golden：flat 下标 i 对应行优先 64×64 矩阵 A[i//64, i%64] = i%128
      lut_flat —— Stage2 右矩阵 B 的 golden（单位阵 I₆₄，host 预填，不经设备计算）
      c_flat  —— Stage2 golden：C = A @ B = A（int32 扩宽累加，因 B=I 故数值不变）
      out_flat —— Stage3+encode golden：out = (C+1) % 64 → int8

    返回 (a_flat, lut_flat, c_flat, out_flat)。
    """
    a_flat = (np.arange(K_S0, dtype=np.int32) % 128).astype(np.int8)
    a_mat = a_flat.reshape(K_ROWS, K_COLS)
    b = np.eye(K_ROWS, dtype=np.int8)
    c = (a_mat.astype(np.int32) @ b.astype(np.int32)).reshape(-1)
    out = ((c + 1) % 64).astype(np.int8)
    return a_flat, b.reshape(-1), c, out


def main() -> None:
    """脚本入口：写 tiling → 落盘设备输入（src/lut）→ 计算并落盘各阶段 golden。"""
    os.makedirs(IN_DIR, exist_ok=True)
    os.makedirs(OUT_DIR, exist_ok=True)

    # TOY_MIX_PASS 优先，PLANAR_MIX_PASS 为历史遗留兼容别名；默认 0（全流程）
    mix_pass = int(os.environ.get("TOY_MIX_PASS", os.environ.get("PLANAR_MIX_PASS", "0")))
    write_tiling(mix_pass)

    # src 全 0：S1 玩具 limb 产出亦 0，随后被 i%128 填数覆盖（golden 不依赖 src 非零，
    # 但仍落盘保持设备侧 CopyIn 的输入契约完整）。
    src = np.zeros(K_SRC_TOTAL, dtype=np.int32)
    src.tofile(os.path.join(IN_DIR, "src.bin"))

    _, lut_flat, c_golden, out_golden = golden_pipeline()
    lut_flat.astype(np.int8).tofile(os.path.join(IN_DIR, "lut.bin"))

    a_golden, _, _, _ = golden_pipeline()
    a_golden.astype(np.int8).tofile(os.path.join(OUT_DIR, "golden_s0.bin"))
    c_golden.astype(np.int32).tofile(os.path.join(OUT_DIR, "golden_mat_c.bin"))
    out_golden.astype(np.int8).tofile(os.path.join(OUT_DIR, "golden_out.bin"))

    print(f"[gen_data] mixPass={mix_pass} src={K_SRC_TOTAL} lut={K_S0} out={K_S0}")


if __name__ == "__main__":
    main()
