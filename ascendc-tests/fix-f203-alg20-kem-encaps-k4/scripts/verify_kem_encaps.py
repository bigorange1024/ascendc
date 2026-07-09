#!/usr/bin/env python3
"""
verify_kem_encaps.py — Alg.20 Encaps 端到端 I/O 对拍。

比较 output/c.bin、K.bin 与 golden_c.bin、golden_K.bin（由 gen_data VERIFY 路径生成）。
仅验字节 max_diff==0；不解释设备实现。
"""
from __future__ import annotations

import sys
from pathlib import Path


def max_diff(a: bytes, b: bytes) -> int:
    """逐字节最大绝对差；长度不等时至少返回 256。"""
    n = min(len(a), len(b))
    m = 0
    for i in range(n):
        m = max(m, abs(a[i] - b[i]))
    if len(a) != len(b):
        return max(m, 256)
    return m


def main() -> None:
    """对拍密文 c 与共享秘密 K。"""
    root = Path(__file__).resolve().parent.parent
    out = root / "output"
    c = (out / "c.bin").read_bytes()
    k = (out / "K.bin").read_bytes()
    gc = (out / "golden_c.bin").read_bytes()
    gk = (out / "golden_K.bin").read_bytes()
    mc = max_diff(c, gc)
    mk = max_diff(k, gk)
    print(f"[verify_kem_encaps] c max={mc} K max={mk}")
    if mc != 0 or mk != 0:
        sys.exit(1)
    print("[verify_kem_encaps] PASS")


if __name__ == "__main__":
    main()
