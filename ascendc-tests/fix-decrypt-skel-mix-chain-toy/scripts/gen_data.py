#!/usr/bin/env python3
"""
gen_data.py — Decrypt 握手骨架 toy 输入生成。

生成：
  input/tiling.bin  — 64B 占位 tiling
  input/src.bin     — 64B 任意合法输入（全 0）
  input/lut.bin     — B = I₃₂ int8（Cube 右矩阵）

不对 ML-KEM golden；verify 只查 magic。
"""
import os
import struct
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IN_DIR = os.path.join(ROOT, "input")

K_ROWS = 16
K_DIM = 32
K_COLS = 32
K_SRC = 64


def main() -> None:
    os.makedirs(IN_DIR, exist_ok=True)
    # tiling：tileLength=n 占位，reserved=0
    payload = struct.pack("<ii", K_COLS, 0)
    payload += b"\x00" * (64 - len(payload))
    with open(os.path.join(IN_DIR, "tiling.bin"), "wb") as f:
        f.write(payload)

    src = np.zeros(K_SRC, dtype=np.uint8)
    src.tofile(os.path.join(IN_DIR, "src.bin"))

    lut = np.eye(K_DIM, dtype=np.int8)  # I₃₂，形状 [32,32]
    lut.tofile(os.path.join(IN_DIR, "lut.bin"))
    print(
        f"[gen_data] wrote tiling/src/lut under {IN_DIR} "
        f"(I_{K_DIM}, A={K_ROWS}x{K_DIM})"
    )


if __name__ == "__main__":
    main()
