#!/usr/bin/env python3
"""
gen_data.py — ER04-cube-ntt-volume 输入生成。

提供：
  - tiling.bin（phase 占位由 Host 运行时改写）
  - seed.bin（32B urandom）+ ref_sha3.bin（文档用；设备不对拍）
  - lut.bin（I₃₂）
  - mac_a.bin / mac_b.bin / mac_acc.bin：GATE 真积木操作数
    每文件 2×AIV × kMacElems=256 × int32（与 tiling.h 已锁参数一致）
    规则：a[i]=i+1+aiv，b[i]=2，acc[i]=0
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
# 与 tiling.h 已锁参数对齐（ER04-TASK）；改参须主控重锁
K_MAC_ELEMS = 256
K_MAC_ROUNDS = 32  # 仅文档标注；设备侧读 tiling::kMacRounds
K_CUBE_ROUNDS = 16  # NTT/INTT 各 16 次真 Mmad；设备侧 tiling::kCubeRounds
N_AIV = 2


def fill_mac_operands() -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """按 Host/设备约定填充双 AIV 的 A/B/ACC（各 int32[256]）。"""
    a = np.zeros((N_AIV, K_MAC_ELEMS), dtype=np.int32)
    b = np.zeros((N_AIV, K_MAC_ELEMS), dtype=np.int32)
    acc = np.zeros((N_AIV, K_MAC_ELEMS), dtype=np.int32)
    for aiv in range(N_AIV):
        for i in range(K_MAC_ELEMS):
            a[aiv, i] = i + 1 + aiv
            b[aiv, i] = 2
            acc[aiv, i] = 0
    return a, b, acc


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

    # GATE MAC：连续排布 [AIV0 | AIV1]，与 tiling::MAC_*_OFF + aiv*kMacVecBytes 一致
    mac_a, mac_b, mac_acc = fill_mac_operands()
    mac_a.tofile(os.path.join(IN_DIR, "mac_a.bin"))
    mac_b.tofile(os.path.join(IN_DIR, "mac_b.bin"))
    mac_acc.tofile(os.path.join(IN_DIR, "mac_acc.bin"))
    mac_bytes = N_AIV * K_MAC_ELEMS * 4
    print(
        f"[gen_data] seed={SEED_BYTES}B ref_sha3={len(ref)}B "
        f"lut=I_{K_DIM} mac_elems={K_MAC_ELEMS} rounds={K_MAC_ROUNDS} "
        f"mac_a/b/acc={mac_bytes}B each (phase set at launch)"
    )


if __name__ == "__main__":
    main()
