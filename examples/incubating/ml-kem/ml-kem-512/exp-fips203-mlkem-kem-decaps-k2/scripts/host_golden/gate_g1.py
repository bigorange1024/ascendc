#!/usr/bin/env python3
"""
gate_g1.py — Alg.15 门控 G1 Host golden：由 c 生成 u'、v'。

流水线位置：verify_gate 在对拍设备 unpack 输出前调用。
语义：ByteDecode₁₀/₄(c₁‖c₂) + Decompress₁₀/₄ → u'[k,n]、v'[n]（int32）。
与设备关系：仅 I/O 等价；实现走 golden_m.unpack_ciphertext，非 AscendC 规格。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from golden_m import unpack_ciphertext


def main() -> None:
    """
    读 case/input/c.bin，写出 golden_u.bin / golden_v.bin 到 out/。

    argv[1]=用例根目录，argv[2]=output 目录。
    """
    case = Path(sys.argv[1])
    out = Path(sys.argv[2])
    # 生产密文 c₁‖c₂（768B）
    c = np.fromfile(case / "input/c.bin", dtype=np.uint8)
    # 与设备 prep unpack 同语义的 Host 参考
    u, v = unpack_ciphertext(bytes(c))
    u.tofile(out / "golden_u.bin")
    v.tofile(out / "golden_v.bin")
    print(f"[gate_g1] u={u.size} v={v.size} coeffs")


if __name__ == "__main__":
    main()
