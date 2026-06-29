#!/usr/bin/env python3
"""
gen_data.py — pass-toy-mix-s123-byteencode-k2 输入与 golden 生成。

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
    """写入 64 字节 tiling.bin：tileLength=64（占位），mixPass 控制 kernel 阶段。"""
    payload = struct.pack("<ii", K_COLS, mix_pass)
    payload += b"\x00" * (64 - len(payload))
    with open(os.path.join(IN_DIR, "tiling.bin"), "wb") as f:
        f.write(payload)


def golden_pipeline() -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """
    一行 NumPy 算完整 golden 链，避免手算 4096 元素。

    返回 (a_flat, lut_flat, c_flat, out_flat)。
    """
    a_flat = (np.arange(K_S0, dtype=np.int32) % 128).astype(np.int8)
    a_mat = a_flat.reshape(K_ROWS, K_COLS)
    b = np.eye(K_ROWS, dtype=np.int8)
    c = (a_mat.astype(np.int32) @ b.astype(np.int32)).reshape(-1)
    out = ((c + 1) % 64).astype(np.int8)
    return a_flat, b.reshape(-1), c, out


def main() -> None:
    os.makedirs(IN_DIR, exist_ok=True)
    os.makedirs(OUT_DIR, exist_ok=True)

    mix_pass = int(os.environ.get("TOY_MIX_PASS", os.environ.get("PLANAR_MIX_PASS", "0")))
    write_tiling(mix_pass)

    # src 全 0：S1 玩具 limb 产出亦 0，随后被 i%128 填数覆盖。
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
