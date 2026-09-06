#!/usr/bin/env python3
"""
gen_data.py — T05-multi-launch-rounds 输入生成。

仅提供合法输入：tiling.bin + lut.bin（I₃₂）。不对算法正确性；无 golden 对拍。
桩哈希与 AIV 体量 scratch 在设备侧初始化/写入；Host 串行 launch 2 轮复用同一输入。
"""
import os
import struct
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IN_DIR = os.path.join(ROOT, "input")

K_DIM = 32
K_COLS = 32


def main() -> None:
    os.makedirs(IN_DIR, exist_ok=True)
    payload = struct.pack("<ii", K_COLS, 0)
    payload += b"\x00" * (64 - len(payload))
    with open(os.path.join(IN_DIR, "tiling.bin"), "wb") as f:
        f.write(payload)

    lut = np.eye(K_DIM, dtype=np.int8)
    lut.tofile(os.path.join(IN_DIR, "lut.bin"))
    print(f"[gen_data] lut=I_{K_DIM} bytes={lut.size} (no golden, 2 launch rounds)")


if __name__ == "__main__":
    main()
