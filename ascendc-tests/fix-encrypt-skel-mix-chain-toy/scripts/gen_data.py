#!/usr/bin/env python3
"""
gen_data.py — Encrypt 骨架 toy 输入生成。

生成：
  input/tiling.bin  — 64B 占位 tiling
  input/src.bin     — 64B 任意合法输入（全 0）
  input/lut.bin     — B = Iₙ int8（Cube 右矩阵；n=32 或 64）

SKEL_HEAVY 读自环境（与 run.sh / 编译宏一致）：
  0 → I₃₂；1 → I₆₄

不对 ML-KEM golden；verify 只查 magic。
"""
import os
import struct
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IN_DIR = os.path.join(ROOT, "input")

K_ROWS = 16
K_SRC = 64


def main() -> None:
    heavy = os.environ.get("SKEL_HEAVY", "0").strip()
    if heavy not in ("0", "1"):
        raise SystemExit(f"SKEL_HEAVY must be 0 or 1, got={heavy!r}")
    k_dim = 64 if heavy == "1" else 32
    k_cols = k_dim

    os.makedirs(IN_DIR, exist_ok=True)
    # tiling：tileLength=n 占位，reserved=0
    payload = struct.pack("<ii", k_cols, 0)
    payload += b"\x00" * (64 - len(payload))
    with open(os.path.join(IN_DIR, "tiling.bin"), "wb") as f:
        f.write(payload)

    src = np.zeros(K_SRC, dtype=np.uint8)
    src.tofile(os.path.join(IN_DIR, "src.bin"))

    lut = np.eye(k_dim, dtype=np.int8)  # Iₙ，形状 [n,n]
    lut.tofile(os.path.join(IN_DIR, "lut.bin"))
    print(
        f"[gen_data] wrote tiling/src/lut under {IN_DIR} "
        f"(I_{k_dim}, SKEL_HEAVY={heavy}, A={K_ROWS}x{k_dim})"
    )


if __name__ == "__main__":
    main()
