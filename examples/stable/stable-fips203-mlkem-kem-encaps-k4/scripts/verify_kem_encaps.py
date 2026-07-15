#!/usr/bin/env python3
"""
verify_kem_encaps.py — 对拍 output/{c,K}.bin 与 golden/（I/O 等价）。

仅验收字节一致；不要求 AscendC 与 liboqs 源码同构。
customspec：stable-fips203-mlkem-kem-encaps-k4-实现方案-customspec.*
registry：docs/specs/fips203-mlkem1024-kem-encaps-baseline-registry.md
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def _max_abs(a: bytes, b: bytes) -> int:
    """逐字节绝对差最大值；空缓冲返回 -1。"""
    return max(abs(x - y) for x, y in zip(a, b, strict=True)) if a else -1


def main() -> int:
    """
    读取 output 与 golden 的 c.bin(1568)/K.bin(32)，打印 max_abs_diff。

    @return 0 PASS；1 数值失败；2 缺文件
    """
    pairs = [
        ("c.bin", 1568),
        ("K.bin", 32),
    ]
    ok = True
    for name, expect in pairs:
        out = ROOT / "output" / name
        g = ROOT / "golden" / name
        if not out.is_file() or not g.is_file():
            print(f"[verify] missing {out} or {g}", file=sys.stderr)
            return 2
        ob, gb = out.read_bytes(), g.read_bytes()
        if len(ob) != expect or len(gb) != expect:
            print(f"[verify] {name} size out={len(ob)} golden={len(gb)} expect={expect}", file=sys.stderr)
            ok = False
            continue
        d = _max_abs(ob, gb)
        print(f"[verify] {name} max_abs_diff={d}")
        if d != 0:
            ok = False
    print("[verify] PASS" if ok else "[verify] FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
