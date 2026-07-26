#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
对拍 output/ 与 golden/（Alg.14 pack 探针）。

流水线位置：run.sh 在 kernel 落盘后调用；验收 mu_embed.bin 与 c.bin 字节级一致。
golden I/O：output/{mu_embed,c}.bin vs golden/{mu_embed,c}.bin。
"""
from __future__ import annotations

import sys
from pathlib import Path


def cmp_files(a: Path, b: Path) -> bool:
    """
    逐字节比较两个文件。
    @param a  实际输出（output/）
    @param b  期望（golden/）
    @return   True 完全一致
    """
    if not a.is_file() or not b.is_file():
        print(f"[FAIL] missing {a} or {b}")
        return False
    da = a.read_bytes()
    db = b.read_bytes()
    if da != db:
        print(f"[FAIL] {a.name}: len {len(da)} vs {len(db)}")
        for i, (x, y) in enumerate(zip(da, db)):
            if x != y:
                print(f"  first diff @ {i}: {x:#04x} vs {y:#04x}")
                break
        return False
    print(f"[OK] {a.name} ({len(da)} bytes)")
    return True


def main() -> int:
    """依次对拍 mu_embed 与 c；全部通过打印 SUCCESS。"""
    root = Path(__file__).resolve().parents[1]
    ok = True
    for name in ("mu_embed.bin", "c.bin"):
        ok = cmp_files(root / "output" / name, root / "golden" / name) and ok
    if not ok:
        return 1
    print("[SUCCESS] output matches golden")
    return 0


if __name__ == "__main__":
    sys.exit(main())
