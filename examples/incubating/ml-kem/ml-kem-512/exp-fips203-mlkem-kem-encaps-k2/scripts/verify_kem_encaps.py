#!/usr/bin/env python3
"""verify_kem_encaps.py — 对拍 ML-KEM-512 Encaps 的 output/c.bin、K.bin 与 golden/。"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def _max_abs(a: bytes, b: bytes) -> int:
    return max(abs(x - y) for x, y in zip(a, b, strict=True)) if a else -1


def main() -> int:
    pairs = [
        ("c.bin", 768),
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
