#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T03 verify：

硬条件（CPU + SIM + NPU）：
  - out 魔数
  - TRACE Host PRE / Host POST_SYNC（单槽）
  - AIV 逻辑事件（NTT SET1 / NTT WAIT3 / GATE SET4 / INTT SET1 / INTT WAIT3）：
    成对槽 AIV0 或 AIV1 任一魔数匹配即可
    （NPU 上 TRACE 可能随机落在 AIV1 而非 AIV0，属 KB X7/X9；勿硬绑 AIV0 槽假红）
  - mat_c_ntt、mat_c_intt 均非全 0（两段有界 Cube）

软条件：成对槽中空的一侧、以及 AIC 直写 TRACE 槽 —— 仅告警不失败。
因果：NTT SET1+WAIT3 + SET4 + INTT SET1+WAIT3 ⇒ 三段握手均完成，否则会死等。
"""
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output"

MAGIC_OUT_OK = 0x54F30033

# Host 单槽：仍硬绑
HARD_HOST = {
    0: 0x484F5354,  # HOST_PRE
    8: 0x484F5355,  # HOST_POST_SYNC
}

# AIV 逻辑事件成对槽：(aiv0_slot, aiv0_magic, aiv1_slot, aiv1_magic, 事件名)
# 背景：NPU TRACE 可能落在 AIV1；结论：同一事件接受任一侧魔数，禁止硬绑 AIV0。
HARD_AIV_PAIRS = [
    (1, 0xA1010001, 2, 0xA1110001, "PRE_SET1_NTT"),
    (5, 0xA1030003, 16, 0xA1130003, "POST_WAIT3_NTT"),
    (6, 0xA1040004, 14, 0xA1140004, "PRE_SET4"),
    (9, 0xA1011001, 15, 0xA1111001, "PRE_SET1_INTT"),
    (12, 0xA1031003, 13, 0xA1131003, "POST_WAIT3_INTT"),
]

# AIC 直写：软告警
SOFT = {
    3: 0xC1010001,  # AIC_POST_WAIT1_NTT
    4: 0xC1030003,  # AIC_PRE_SET3_NTT
    7: 0xC1040004,  # AIC_POST_WAIT4
    10: 0xC1011001,  # AIC_POST_WAIT1_INTT
    11: 0xC1031003,  # AIC_PRE_SET3_INTT
}


def _check_aiv_pair(vals, s0: int, m0: int, s1: int, m1: int, label: str) -> bool:
    """同一逻辑事件：AIV0 或 AIV1 对应槽任一非零且魔数匹配即过。"""
    g0, g1 = vals[s0], vals[s1]
    ok0 = g0 == m0
    ok1 = g1 == m1
    if not ok0 and not ok1:
        print(
            f"[FAIL] TRACE {label}: AIV0[{s0}]=0x{g0:08X} (expect 0x{m0:08X}) "
            f"且 AIV1[{s1}]=0x{g1:08X} (expect 0x{m1:08X}) — 成对槽皆未命中",
            file=sys.stderr,
        )
        return False
    if not ok0:
        print(
            f"[WARN] TRACE[{s0}]=0x{g0:08X} (AIV0 {label} 空；已由 AIV1[{s1}]=0x{g1:08X} 验收，"
            f"不绑死 AIV0 / KB X7/X9)"
        )
    if not ok1:
        print(
            f"[WARN] TRACE[{s1}]=0x{g1:08X} (AIV1 {label} 空；已由 AIV0[{s0}] 验收；"
            f"SIM 上 AIV1 标量 TRACE 常空)"
        )
    return True


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
    # 需覆盖到槽 16（AIV1_POST_WAIT3_NTT）
    if len(tr) < 17 * 4:
        print("[FAIL] trace.bin too short", file=sys.stderr)
        return 1
    vals = list(struct.unpack_from("<17I", tr, 0))
    for slot, want in HARD_HOST.items():
        got = vals[slot]
        if got != want:
            print(f"[FAIL] TRACE[{slot}]=0x{got:08X} expect 0x{want:08X}", file=sys.stderr)
            return 1
    for s0, m0, s1, m1, label in HARD_AIV_PAIRS:
        if not _check_aiv_pair(vals, s0, m0, s1, m1, label):
            return 1
    for slot, want in SOFT.items():
        got = vals[slot]
        if got != want:
            print(f"[WARN] TRACE[{slot}]=0x{got:08X} (soft; SIM 上 AIC/AIV1 标量 TRACE 常空)")
    if not _non_zero(OUT / "mat_c_ntt.bin", "mat_c_ntt.bin"):
        return 1
    if not _non_zero(OUT / "mat_c_intt.bin", "mat_c_intt.bin"):
        return 1
    print("[SUCCESS] NTT(1/3)+GATE(4)+INTT(1/3) causal + dual Cube — out+TRACE+mat_c ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
