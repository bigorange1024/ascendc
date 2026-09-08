#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""RB-T06 verify：output/c.bin 与 golden_c.bin 逐字节一致；长度 1568。"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output"
C_LEN = 1568
C1_LEN = 1408


def main() -> int:
    got_p = OUT / "c.bin"
    gold_p = OUT / "golden_c.bin"
    if not got_p.is_file() or not gold_p.is_file():
        print("[FAIL] missing c.bin or golden_c.bin", file=sys.stderr)
        return 1
    got = got_p.read_bytes()
    gold = gold_p.read_bytes()
    if len(got) != C_LEN or len(gold) != C_LEN:
        print(f"[FAIL] len got={len(got)} gold={len(gold)} expect={C_LEN}", file=sys.stderr)
        return 1
    if got != gold:
        mism = sum(1 for a, b in zip(got, gold) if a != b)
        # 定位首错字节，区分 c1/c2
        for i, (a, b) in enumerate(zip(got, gold)):
            if a != b:
                region = "c1" if i < C1_LEN else "c2"
                print(
                    f"[FAIL] first mismatch @{i} ({region}) got=0x{a:02X} gold=0x{b:02X}; "
                    f"total_mism={mism}",
                    file=sys.stderr,
                )
                return 1
    print(f"[SUCCESS] c[1568] matches golden (c1=1408B @0, c2=160B @1408)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
