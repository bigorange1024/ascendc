#!/usr/bin/env python3
"""
gate_g2.py — Alg.15 门控 G2 Host golden：ŝ、û。

流水线位置：verify_gate 在 G1 通过后调用。
语义：
  ŝ ← ByteDecode₁₂(dk_PKE)（k×n int32）
  û ← Stage123 NTT(u')（与设备 ntt_u 同 LUT 数学契约）
与设备关系：仅对拍中间态；禁止把本脚本当 AscendC 实现规格。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from f203_ref_common import stage123_transform
from golden_m import decode_s_hat, unpack_ciphertext


def main() -> None:
    """读 dk+c，写出 golden_s_hat.bin / golden_u_hat.bin。"""
    case = Path(sys.argv[1])
    out = Path(sys.argv[2])
    dk = bytes(np.fromfile(case / "input/dk_pke.bin", dtype=np.uint8))
    c = bytes(np.fromfile(case / "input/c.bin", dtype=np.uint8))
    # 行 5：私钥解码
    s_hat = decode_s_hat(dk)
    # 先 unpack 得 u'，再 NTT（与设备 prep→NTT 顺序一致）
    u, _ = unpack_ciphertext(c)
    u_hat = stage123_transform(u, "ntt")
    s_hat.astype(np.int32).tofile(out / "golden_s_hat.bin")
    u_hat.astype(np.int32).tofile(out / "golden_u_hat.bin")
    print("[gate_g2] s_hat u_hat golden OK")


if __name__ == "__main__":
    main()
