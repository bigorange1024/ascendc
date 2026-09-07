#!/usr/bin/env python3
"""
gen_data.py — ER01-encrypt-shaped-2launch-skel 输入生成。

提供：tiling.bin（phase 占位由 Host 运行时改写）+ seed.bin（32B urandom）+ lut.bin（I₃₂）。
seed 的 SHA3-256 参考写入 ref_sha3.bin（文档用；设备不对拍）。
"""
import hashlib
import os
import struct

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IN_DIR = os.path.join(ROOT, "input")

K_DIM = 32
K_COLS = 32
SEED_BYTES = 32


def main() -> None:
    os.makedirs(IN_DIR, exist_ok=True)
    # tileLength=K_COLS, phase=0（Host 两次 launch 会改写 phase）
    payload = struct.pack("<ii", K_COLS, 0)
    payload += b"\x00" * (64 - len(payload))
    with open(os.path.join(IN_DIR, "tiling.bin"), "wb") as f:
        f.write(payload)

    seed = os.urandom(SEED_BYTES)
    with open(os.path.join(IN_DIR, "seed.bin"), "wb") as f:
        f.write(seed)

    ref = hashlib.sha3_256(seed).digest()
    with open(os.path.join(IN_DIR, "ref_sha3.bin"), "wb") as f:
        f.write(ref)

    lut = np.eye(K_DIM, dtype=np.int8)
    lut.tofile(os.path.join(IN_DIR, "lut.bin"))
    print(
        f"[gen_data] seed={SEED_BYTES}B ref_sha3={len(ref)}B "
        f"lut=I_{K_DIM} (MAC operands filled on host; phase set at launch)"
    )


if __name__ == "__main__":
    main()
