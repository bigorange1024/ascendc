#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T30 verify：K ≡ liboqs Decaps；Decrypt/Encaps TRACE；out magic；launch_count==2。
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

import numpy as np

_CASE = Path(__file__).resolve().parent.parent
OUT = _CASE / "output"

MAGIC_OUT = 0x54333032  # "T302"

DEC_HARD = {
    1: 0x50524550,  # PREP_DONE
    6: 0xA1030003,  # POST_WAIT3_NTT
    7: 0x4E545444,  # NTT_DOT
    12: 0xA1031003,  # POST_WAIT3_INTT
    13: 0x4D455854,  # EXTRACT
}

ENC_HARD = {
    1: 0x50524550,  # PREP_DONE
    27: 0x47303132,  # G_DONE
    6: 0xA1030003,  # POST_WAIT3_NTT
    8: 0xA1040004,  # PRE_SET4 GATE
    14: 0xA1031003,  # POST_WAIT3_INTT
    16: 0x5041434B,  # PACK
}


def main() -> int:
    if (OUT / "BLOCKED").is_file():
        print(f"[FAIL] BLOCKED: {(OUT / 'BLOCKED').read_text(encoding='utf-8').strip()}", file=sys.stderr)
        return 2

    backend = OUT / "cross_backend.txt"
    if not backend.is_file() or backend.read_text(encoding="utf-8").strip() != "liboqs":
        print("[FAIL] cross_backend 非 liboqs — 禁止假绿", file=sys.stderr)
        return 2

    launch_path = OUT / "launch_count.txt"
    if not launch_path.is_file():
        print("[FAIL] missing launch_count.txt", file=sys.stderr)
        return 6
    launch_n = int(launch_path.read_text(encoding="utf-8").strip())
    if launch_n != 2:
        print(f"[FAIL] launch_count={launch_n} want 2", file=sys.stderr)
        return 6
    print("[PASS] Host launches=2")

    out_bin = (OUT / "out.bin").read_bytes()
    magic = struct.unpack_from("<I", out_bin, 0)[0]
    if magic != MAGIC_OUT:
        print(f"[FAIL] out magic 0x{magic:08X} want 0x{MAGIC_OUT:08X}")
        return 3
    print(f"[PASS] out magic 0x{MAGIC_OUT:08X}")

    k = (OUT / "K.bin").read_bytes()
    golden = (OUT / "golden_K.bin").read_bytes()
    if len(k) != 32 or len(golden) != 32 or k != golden:
        mism = sum(1 for a, b in zip(k, golden) if a != b) + abs(len(k) - len(golden))
        print(f"[FAIL] K != golden_K mism~={mism}")
        return 1
    print("[PASS_IO] K == golden_K (liboqs Decaps)")

    tr_dec = np.fromfile(OUT / "trace_dec.bin", dtype=np.uint32)
    for slot, want in DEC_HARD.items():
        got = int(tr_dec[slot]) if slot < len(tr_dec) else 0
        if got != want:
            print(f"[FAIL_SYNC] DEC TRACE[{slot}]=0x{got:08X} want 0x{want:08X}")
            return 4
    print("[PASS_SYNC] Decrypt PREP→NTT→INTT→EXTRACT")

    tr_enc = np.fromfile(OUT / "trace_enc.bin", dtype=np.uint32)
    for slot, want in ENC_HARD.items():
        got = int(tr_enc[slot]) if slot < len(tr_enc) else 0
        if got != want:
            print(f"[FAIL_SYNC] ENC TRACE[{slot}]=0x{got:08X} want 0x{want:08X}")
            return 5
    print("[PASS_SYNC] Reenc PREP→G→NTT→GATE→INTT→PACK")

    c_in = (_CASE / "input" / "c.bin").read_bytes()
    c_prime = (OUT / "c_prime.bin").read_bytes()
    if c_in == c_prime:
        print("[PASS] c' == c (accept path)")
    else:
        print("[WARN] c' != c — 走了拒绝分支；K 仍须 ≡ liboqs")

    print("[SUCCESS] PASS_SYNC + PASS_IO — RB-T30 decaps 2-launch")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
