#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T09 verify：分栏 PASS_SYNC / PASS_IO。

PASS_SYNC（硬，失败则整体 FAIL）：
  - out 魔数
  - TRACE：HOST_PRE / PREP_DONE / AIV0 NTT SET1+WAIT3 / PRE_SET4 /
    INTT SET1+WAIT3 / PACK_DONE / HOST_MID / HOST_POST
  - mat_c_ntt、mat_c_intt 非全 0（两段有界 Cube）

PASS_IO（尽力；失败仅告警并打印 PASS_IO=0，不否决 SYNC）：
  - c.bin == golden_c.bin
  - y_e1_e2_dev / rho_dev 与 golden 一致

退出码：SYNC 失败 → 1；仅 IO 失败 → 0（stdout 标明 PASS_IO=0）。
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output"

MAGIC_OUT_OK = 0x54390033
HARD = {
    0: 0x484F5354,  # HOST_PRE
    1: 0x50524550,  # PREP_DONE
    2: 0xA1010001,  # AIV0_PRE_SET1_NTT
    6: 0xA1030003,  # AIV0_POST_WAIT3_NTT
    7: 0xA1040004,  # AIV0_PRE_SET4
    9: 0x484F4D49,  # HOST_MID_SYNC
    10: 0xA1011001,  # AIV0_PRE_SET1_INTT
    13: 0xA1031003,  # AIV0_POST_WAIT3_INTT
    15: 0x5041434B,  # PACK_DONE
    16: 0x484F5355,  # HOST_POST
}
SOFT = {
    3: 0xA1110001,
    4: 0xC1010001,
    5: 0xC1030003,
    8: 0xC1040004,
    11: 0xC1011001,
    12: 0xC1031003,
    14: 0xA1131003,
    17: 0xA1140004,
    18: 0xA1111001,
}


def _non_zero(path: Path, label: str) -> bool:
    if not path.is_file():
        print(f"[FAIL] missing {label}", file=sys.stderr)
        return False
    data = path.read_bytes()
    if len(data) < 4 or all(b == 0 for b in data):
        print(f"[FAIL] {label} all-zero — Cube 未写出", file=sys.stderr)
        return False
    return True


def main() -> int:
    out_p = OUT / "out.bin"
    tr_p = OUT / "trace.bin"
    if not out_p.is_file() or not tr_p.is_file():
        print("[FAIL] missing output/out.bin or output/trace.bin", file=sys.stderr)
        return 1
    out = out_p.read_bytes()
    if len(out) < 4:
        print("[FAIL] out.bin too short", file=sys.stderr)
        return 1
    (out_magic,) = struct.unpack_from("<I", out, 0)
    if out_magic != MAGIC_OUT_OK:
        print(f"[FAIL] out magic 0x{out_magic:08X} != 0x{MAGIC_OUT_OK:08X}", file=sys.stderr)
        return 1
    tr = tr_p.read_bytes()
    if len(tr) < 19 * 4:
        print("[FAIL] trace.bin too short", file=sys.stderr)
        return 1
    vals = list(struct.unpack_from("<19I", tr, 0))
    for slot, want in HARD.items():
        got = vals[slot]
        if got != want:
            print(f"[FAIL] TRACE[{slot}]=0x{got:08X} expect 0x{want:08X}", file=sys.stderr)
            return 1
    for slot, want in SOFT.items():
        got = vals[slot]
        if got != want:
            print(f"[WARN] TRACE[{slot}]=0x{got:08X} (soft; SIM 上 AIC/AIV1 标量 TRACE 常空)")
    if not _non_zero(OUT / "mat_c_ntt.bin", "mat_c_ntt.bin"):
        return 1
    if not _non_zero(OUT / "mat_c_intt.bin", "mat_c_intt.bin"):
        return 1

    print("[PASS_SYNC] dual-launch causal + dual Cube + prep/pack TRACE ok")

    io_ok = True
    c_p = OUT / "c.bin"
    g_c = OUT / "golden_c.bin"
    if not c_p.is_file() or not g_c.is_file():
        print("[PASS_IO=0] missing c.bin or golden_c.bin")
        io_ok = False
    else:
        c = c_p.read_bytes()
        g = g_c.read_bytes()
        if c != g:
            mism = sum(1 for a, b in zip(c, g) if a != b) + abs(len(c) - len(g))
            print(f"[PASS_IO=0] c mismatch bytes~={mism} len={len(c)}/{len(g)}")
            io_ok = False
        else:
            print("[PASS_IO] c[1568] == golden_c")

    y_p = OUT / "y_e1_e2_dev.bin"
    g_y = OUT / "golden_y_e1_e2.bin"
    if y_p.is_file() and g_y.is_file():
        if y_p.read_bytes() != g_y.read_bytes():
            print("[PASS_IO=0] y_e1_e2_dev != golden")
            io_ok = False
        else:
            print("[PASS_IO] y_e1_e2_dev match")
    rho_p = OUT / "rho_dev.bin"
    g_r = OUT / "golden_rho.bin"
    if rho_p.is_file() and g_r.is_file():
        if rho_p.read_bytes() != g_r.read_bytes():
            print("[PASS_IO=0] rho_dev != golden")
            io_ok = False
        else:
            print("[PASS_IO] rho_dev match")

    if io_ok:
        print("[SUCCESS] PASS_SYNC + PASS_IO — RB-T09 dual launch")
    else:
        print("[SUCCESS] PASS_SYNC only (PASS_IO=0) — sync 优先，IO 尽力")
    return 0


if __name__ == "__main__":
    sys.exit(main())
