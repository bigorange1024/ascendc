#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T27 verify：主验收 K_decaps ≡ K_encaps；可选 ≡ liboqs golden_K；TRACE 因果。
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

import numpy as np

_CASE = Path(__file__).resolve().parent.parent
OUT = _CASE / "output"

MAGIC_OUT = 0x543F001B

ENC_HARD = {
    1: 0x50524550,  # PREP_DONE
    27: 0x47303132,  # G_DONE
    6: 0xA1030003,  # POST_WAIT3_NTT
    8: 0xA1040004,  # PRE_SET4 GATE
    14: 0xA1031003,  # POST_WAIT3_INTT
    16: 0x5041434B,  # PACK
}

DEC_HARD = {
    1: 0x50524550,  # PREP_DONE
    6: 0xA1030003,  # POST_WAIT3_NTT
    7: 0x4E545444,  # NTT_DOT
    12: 0xA1031003,  # POST_WAIT3_INTT
    13: 0x4D455854,  # EXTRACT
}


def main() -> int:
    if (OUT / "BLOCKED").is_file():
        print(f"[FAIL] BLOCKED: {(OUT / 'BLOCKED').read_text(encoding='utf-8').strip()}", file=sys.stderr)
        return 2

    backend = OUT / "cross_backend.txt"
    if not backend.is_file() or backend.read_text(encoding="utf-8").strip() != "liboqs":
        print("[FAIL] cross_backend 非 liboqs — 禁止假绿", file=sys.stderr)
        return 2

    out_bin = (OUT / "out.bin").read_bytes()
    magic = struct.unpack_from("<I", out_bin, 0)[0]
    if magic != MAGIC_OUT:
        print(f"[FAIL] out magic 0x{magic:08X} want 0x{MAGIC_OUT:08X}")
        return 3
    print(f"[PASS] out magic 0x{MAGIC_OUT:08X}")

    k_enc = (OUT / "K_encaps.bin").read_bytes()
    k_dec = (OUT / "K_decaps.bin").read_bytes()
    if len(k_enc) != 32 or len(k_dec) != 32 or k_enc != k_dec:
        mism = sum(1 for a, b in zip(k_enc, k_dec) if a != b) + abs(len(k_enc) - len(k_dec))
        print(f"[FAIL] K_decaps != K_encaps mism~={mism}")
        return 1
    print("[PASS_IO] K_decaps == K_encaps (device RT)")

    golden = (OUT / "golden_K.bin").read_bytes()
    if len(golden) != 32 or k_enc != golden:
        mism = sum(1 for a, b in zip(k_enc, golden) if a != b) + abs(len(k_enc) - len(golden))
        print(f"[FAIL] K_encaps != golden_K (liboqs) mism~={mism}")
        return 1
    print("[PASS_CROSS] K_encaps == golden_K (liboqs Encaps)")

    c_enc = (OUT / "c_encaps.bin").read_bytes()
    golden_c = (OUT / "golden_c.bin").read_bytes()
    if c_enc == golden_c:
        print("[PASS_CROSS] c_encaps == golden_c (liboqs)")
    else:
        mism = sum(1 for a, b in zip(c_enc, golden_c) if a != b) + abs(len(c_enc) - len(golden_c))
        print(f"[FAIL] c_encaps != golden_c mism~={mism}")
        return 1

    tr_enc = np.fromfile(OUT / "trace_encaps.bin", dtype=np.uint32)
    for slot, want in ENC_HARD.items():
        got = int(tr_enc[slot]) if slot < len(tr_enc) else 0
        if got != want:
            print(f"[FAIL_SYNC] ENCAPS TRACE[{slot}]=0x{got:08X} want 0x{want:08X}")
            return 4
    print("[PASS_SYNC] Encaps PREP→G→NTT→GATE→INTT→PACK")

    tr_dec = np.fromfile(OUT / "trace_dec.bin", dtype=np.uint32)
    for slot, want in DEC_HARD.items():
        got = int(tr_dec[slot]) if slot < len(tr_dec) else 0
        if got != want:
            print(f"[FAIL_SYNC] DEC TRACE[{slot}]=0x{got:08X} want 0x{want:08X}")
            return 5
    print("[PASS_SYNC] Decrypt PREP→NTT→INTT→EXTRACT")

    tr_re = np.fromfile(OUT / "trace_reenc.bin", dtype=np.uint32)
    for slot, want in ENC_HARD.items():
        got = int(tr_re[slot]) if slot < len(tr_re) else 0
        if got != want:
            print(f"[FAIL_SYNC] REENC TRACE[{slot}]=0x{got:08X} want 0x{want:08X}")
            return 6
    print("[PASS_SYNC] Reenc PREP→G→NTT→GATE→INTT→PACK")

    c_prime = (OUT / "c_prime.bin").read_bytes()
    if c_enc == c_prime:
        print("[PASS] c' == c_encaps (accept path)")
    else:
        print("[WARN] c' != c_encaps — 拒绝分支；K 仍须 ≡ Encaps")

    print("[SUCCESS] PASS_SYNC + PASS_IO + PASS_CROSS — RB-T27 encaps→decaps RT")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
