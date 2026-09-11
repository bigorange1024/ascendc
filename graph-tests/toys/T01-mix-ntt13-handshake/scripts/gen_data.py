#!/usr/bin/env python3
"""
gen_data.py — T01-mix-ntt13-handshake 输入生成。

仅提供合法输入：tiling.bin + lut.bin（I₃₂）。不对算法正确性；无 golden 对拍。
桩哈希在设备侧 SetValue，不需要 src。
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
    # TilingData：tileLength=32 占位，reserved=0；补齐 64B
    payload = struct.pack("<ii", K_COLS, 0)
    payload += b"\x00" * (64 - len(payload))
    with open(os.path.join(IN_DIR, "tiling.bin"), "wb") as f:
        f.write(payload)

    # 右矩阵 B = I₃₂ int8，供极轻 MMAD
    lut = np.eye(K_DIM, dtype=np.int8)
    lut.tofile(os.path.join(IN_DIR, "lut.bin"))
    print(f"[gen_data] lut=I_{K_DIM} bytes={lut.size} (no golden)")


if __name__ == "__main__":
    main()
