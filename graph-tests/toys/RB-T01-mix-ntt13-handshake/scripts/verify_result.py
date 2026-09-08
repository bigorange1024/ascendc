#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T01 verify：

硬条件（CPU + SIM）：
  - out 魔数
  - TRACE Host PRE / AIV0 PRE_SET1 / AIV0 POST_WAIT3 / Host POST_SYNC
  - mat_c 非全 0（证明 AIC 极轻 Cube 在 WAIT1→SET3 之间跑过）

软条件：AIV1 / AIC 直写 TRACE 槽在 CAModel 上常为 0（CPU 孪生可全绿）——仅告警不失败。
因果：SET1+WAIT3 ⇒ WAIT1+SET3 必已发生，否则会死等。
"""
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output"

MAGIC_OUT_OK = 0x54F10013
HARD = {
    0: 0x484F5354,  # HOST_PRE
    1: 0xA1010001,  # AIV0_PRE_SET1 → SET1
    5: 0xA1030003,  # AIV0_POST_WAIT3 → after WAIT3
    7: 0x484F5355,  # HOST_POST_SYNC → sync 后
}
SOFT = {
    2: 0xA1110001,
    3: 0xC1010001,  # WAIT1 direct
    4: 0xC1030003,  # SET3 direct
    6: 0xA1130003,
}


def main() -> int:
    out_p = OUT / "out.bin"
    tr_p = OUT / "trace.bin"
    mc_p = OUT / "mat_c.bin"
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
    if len(tr) < 8 * 4:
        print("[FAIL] trace.bin too short", file=sys.stderr)
        return 1
    vals = list(struct.unpack_from("<8I", tr, 0))
    for slot, want in HARD.items():
        got = vals[slot]
        if got != want:
            print(f"[FAIL] TRACE[{slot}]=0x{got:08X} expect 0x{want:08X}", file=sys.stderr)
            return 1
    for slot, want in SOFT.items():
        got = vals[slot]
        if got != want:
            print(f"[WARN] TRACE[{slot}]=0x{got:08X} (soft; SIM 上 AIC/AIV1 标量 TRACE 常空)")
    if not mc_p.is_file():
        print("[FAIL] missing output/mat_c.bin (Cube 产出)", file=sys.stderr)
        return 1
    mc = mc_p.read_bytes()
    if len(mc) < 4 or all(b == 0 for b in mc):
        print("[FAIL] mat_c all-zero — Cube 未写出", file=sys.stderr)
        return 1
    print("[SUCCESS] SET1 / WAIT1+SET3(causal+Cube) / WAIT3 / sync 后 — out+TRACE+mat_c ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
