#!/usr/bin/env python3
"""
gen_data.py — E01 toy 输入：tiling 占位 + src 全 0。
无 LUT/业务；verify 看 Host TRACE 与 magic。
"""
import os
import struct
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IN_DIR = os.path.join(ROOT, "input")


def main() -> None:
    os.makedirs(IN_DIR, exist_ok=True)
    payload = struct.pack("<ii", 0, 0)
    payload += b"\x00" * (64 - len(payload))
    with open(os.path.join(IN_DIR, "tiling.bin"), "wb") as f:
        f.write(payload)
    np.zeros(64, dtype=np.uint8).tofile(os.path.join(IN_DIR, "src.bin"))
    print(f"[gen_data] wrote tiling/src under {IN_DIR} (E01 toy)")


if __name__ == "__main__":
    main()
