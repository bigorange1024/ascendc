#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-D09 verify：单 launch Decrypt — m ≡ golden_m；out magic D091；TRACE PREP+NTT+INTT。

硬条件：
  - cross_backend.txt == liboqs
  - m.bin ≡ golden_m.bin（逐字节）
  - out.bin magic == 0x44303931
  - TRACE 硬槽：PREP / POST_WAIT3_NTT / NTT_DOT / POST_WAIT3_INTT / EXTRACT
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output"

MAGIC_OUT = 0x44303931  # "D091"

# TRACE 硬槽（相对 d09 tiling）
HARD_TRACE = {
    1: 0x50524550,  # PREP_DONE "PREP"
    6: 0xA1030003,  # AIV0 POST_WAIT3_NTT
    7: 0x4E545444,  # NTT_DOT "NTTD"
    12: 0xA1031003,  # AIV0 POST_WAIT3_INTT
    13: 0x4D455854,  # EXTRACT "MEXT"
}


def main() -> int:
    backend_path = OUT / "cross_backend.txt"
    if not backend_path.is_file():
        print("[FAIL] missing cross_backend.txt", file=sys.stderr)
        return 2
    backend = backend_path.read_text(encoding="utf-8").strip()
    if backend != "liboqs":
        print(f"[FAIL] cross_backend={backend!r} — 要求 liboqs（禁止假绿）", file=sys.stderr)
        return 2

    m_got = OUT / "m.bin"
    m_gold = OUT / "golden_m.bin"
    if not m_got.is_file() or not m_gold.is_file():
        print("[FAIL] missing m.bin or golden_m.bin", file=sys.stderr)
        return 1
    a = m_got.read_bytes()
    b = m_gold.read_bytes()
    if len(a) != 32 or len(b) != 32:
        print(f"[FAIL] m len got={len(a)} golden={len(b)} expect 32", file=sys.stderr)
        return 1
    pass_io = a == b
    if pass_io:
        print(f"[OK] m max_abs=0 n=32 oracle={backend}")
    else:
        mism = sum(1 for x, y in zip(a, b) if x != y)
        print(f"[FAIL] m mism={mism}/32 oracle={backend}", file=sys.stderr)

    out_bin = OUT / "out.bin"
    if not out_bin.is_file():
        print("[FAIL] missing out.bin", file=sys.stderr)
        return 3
    (got_magic,) = struct.unpack_from("<I", out_bin.read_bytes(), 0)
    pass_magic = got_magic == MAGIC_OUT
    if pass_magic:
        print(f"[OK] out magic 0x{got_magic:08X}")
    else:
        print(f"[FAIL] out magic 0x{got_magic:08X} want 0x{MAGIC_OUT:08X}", file=sys.stderr)

    tr_path = OUT / "trace.bin"
    if not tr_path.is_file():
        print("[FAIL] missing trace.bin", file=sys.stderr)
        return 4
    tr = tr_path.read_bytes()
    if len(tr) < 14 * 4:
        print("[FAIL] trace.bin too short", file=sys.stderr)
        return 4
    vals = list(struct.unpack_from("<24I", tr + b"\x00" * (96 - len(tr)), 0))
    pass_sync = True
    for slot, want in HARD_TRACE.items():
        got = vals[slot] if slot < len(vals) else 0
        if got != want:
            print(f"[FAIL_SYNC] TRACE[{slot}]=0x{got:08X} want 0x{want:08X}", file=sys.stderr)
            pass_sync = False
    if pass_sync:
        print("[PASS_SYNC] PREP→NTT→INTT→EXTRACT")

    # Cube 诊断：非全零
    for name in ("mat_c_ntt.bin", "mat_c_intt.bin"):
        p = OUT / name
        if p.is_file():
            data = p.read_bytes()
            if len(data) >= 4 and not all(b == 0 for b in data):
                print(f"[OK] {name} non-zero (Cube)")
            else:
                print(f"[WARN] {name} all-zero or missing content")

    if pass_sync and pass_io and pass_magic:
        print(f"[SUCCESS] RB-D09 PASS_SYNC + PASS_IO oracle={backend}")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
