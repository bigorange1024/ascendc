#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T25 verify：m 与 liboqs golden 对拍；TRACE 因果链；out magic。
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

import numpy as np

_CASE = Path(__file__).resolve().parent.parent

MAGIC_OUT_OK = 0x543F0019
MAGIC_PREP_DONE = 0x50524550
MAGIC_AIV0_POST_WAIT3_NTT = 0xA1030003
MAGIC_AIV0_NTT_DOT_DONE = 0x4E545444
MAGIC_AIV0_POST_WAIT3_INTT = 0xA1031003
MAGIC_AIV0_EXTRACT_DONE = 0x4D455854

SLOT_PREP_DONE = 1
SLOT_AIV0_POST_WAIT3_NTT = 6
SLOT_AIV0_NTT_DOT_DONE = 7
SLOT_AIV0_POST_WAIT3_INTT = 12
SLOT_AIV0_EXTRACT_DONE = 13


def main() -> int:
    out = _CASE / "output"
    m = (out / "m.bin").read_bytes()
    golden = (out / "golden_m.bin").read_bytes()
    if len(m) != 32 or len(golden) != 32:
        print(f"[FAIL] m/golden size m={len(m)} golden={len(golden)}")
        return 1
    if m != golden:
        idx = next(i for i, (a, b) in enumerate(zip(m, golden)) if a != b)
        print(f"[FAIL] m != golden_m @ byte {idx}: {m[idx]:02x} vs {golden[idx]:02x}")
        return 2
    print("[PASS_IO] m == golden_m (liboqs PKE Decrypt)")

    out_bin = (out / "out.bin").read_bytes()
    magic = struct.unpack_from("<I", out_bin, 0)[0]
    if magic != MAGIC_OUT_OK:
        print(f"[FAIL] out magic 0x{magic:08X} want 0x{MAGIC_OUT_OK:08X}")
        return 3
    print(f"[PASS] out magic 0x{MAGIC_OUT_OK:08X}")

    tr = np.fromfile(out / "trace.bin", dtype=np.uint32)
    checks = [
        (SLOT_PREP_DONE, MAGIC_PREP_DONE, "PREP"),
        (SLOT_AIV0_POST_WAIT3_NTT, MAGIC_AIV0_POST_WAIT3_NTT, "NTT_WAIT3"),
        (SLOT_AIV0_NTT_DOT_DONE, MAGIC_AIV0_NTT_DOT_DONE, "NTT_DOT"),
        (SLOT_AIV0_POST_WAIT3_INTT, MAGIC_AIV0_POST_WAIT3_INTT, "INTT_WAIT3"),
        (SLOT_AIV0_EXTRACT_DONE, MAGIC_AIV0_EXTRACT_DONE, "EXTRACT"),
    ]
    for slot, want, name in checks:
        got = int(tr[slot]) if slot < len(tr) else 0
        if got != want:
            print(f"[FAIL_SYNC] TRACE[{slot}] {name}=0x{got:08X} want 0x{want:08X}")
            return 4
    print("[PASS_SYNC] PREP→NTT→INTT→EXTRACT TRACE linked")
    print("[SUCCESS] verify_result RB-T25")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
