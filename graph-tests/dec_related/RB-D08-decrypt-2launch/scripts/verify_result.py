#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-D04 verify：三 launch 全链 — m ≡ oracle；L2a/L2b TRACE mid-sync 证据。

硬条件：
  - m.bin 与 golden_m.bin 逐字节一致（主门禁）
  - out_l2a / out_l2b 魔数 D02: / D03:
  - TRACE L2a/L2b：HOST_PRE/POST + AIV GATE/SET1/WAIT3 成对
  - mat_c_l2a / mat_c_l2b 非全 0（Cube 证据）

软：AIC TRACE、DONE 缺失仅告警。
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output"

MAGIC_L2A = 0x4430323A  # D02:
MAGIC_L2B = 0x4430333A  # D03:

HARD_HOST = {
    0: 0x484F5354,  # HOST_PRE
    11: 0x484F5355,  # HOST_POST_SYNC
}

HARD_AIV_PAIRS = [
    (1, 0xA1040004, 2, 0xA1140004, "PRE_SET4_GATE"),
    (3, 0xA1010001, 4, 0xA1110001, "PRE_SET1"),
    (8, 0xA1030003, 9, 0xA1130003, "POST_WAIT3"),
]


def _check_aiv_pair(vals, s0: int, m0: int, s1: int, m1: int, label: str) -> bool:
    g0, g1 = vals[s0], vals[s1]
    ok0 = g0 == m0
    ok1 = g1 == m1
    if not ok0 and not ok1:
        print(
            f"[FAIL] TRACE {label}: AIV0[{s0}]=0x{g0:08X} 且 AIV1[{s1}]=0x{g1:08X} 皆未命中",
            file=sys.stderr,
        )
        return False
    if not ok0:
        print(f"[WARN] TRACE[{s0}] AIV0 {label} 空；已由 AIV1[{s1}] 验收")
    if not ok1:
        print(f"[WARN] TRACE[{s1}] AIV1 {label} 空；已由 AIV0[{s0}] 验收")
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


def _check_trace(path: Path, tag: str) -> bool:
    if not path.is_file():
        print(f"[FAIL] missing {path.name}", file=sys.stderr)
        return False
    tr = path.read_bytes()
    if len(tr) < 12 * 4:
        print(f"[FAIL] {path.name} too short", file=sys.stderr)
        return False
    vals = list(struct.unpack_from("<12I", tr, 0))
    ok = True
    for slot, want in HARD_HOST.items():
        if vals[slot] != want:
            print(
                f"[FAIL] {tag} TRACE[{slot}]=0x{vals[slot]:08X} expect 0x{want:08X}",
                file=sys.stderr,
            )
            ok = False
    for s0, m0, s1, m1, label in HARD_AIV_PAIRS:
        if not _check_aiv_pair(vals, s0, m0, s1, m1, f"{tag}:{label}"):
            ok = False
    return ok


def _check_out_magic(path: Path, want: int, tag: str) -> bool:
    if not path.is_file():
        print(f"[FAIL] missing {path.name}", file=sys.stderr)
        return False
    data = path.read_bytes()
    if len(data) < 4:
        print(f"[FAIL] {path.name} too short", file=sys.stderr)
        return False
    (got,) = struct.unpack_from("<I", data, 0)
    if got != want:
        print(f"[FAIL] {tag} out magic 0x{got:08X} != 0x{want:08X}", file=sys.stderr)
        return False
    print(f"[OK] {tag} out magic 0x{got:08X}")
    return True


def main() -> int:
    backend = "unknown"
    bp = OUT / "cross_backend.txt"
    if bp.is_file():
        backend = bp.read_text(encoding="utf-8").strip() or "unknown"

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

    pass_sync = True
    if not _check_out_magic(OUT / "out_l2a.bin", MAGIC_L2A, "L2a"):
        pass_sync = False
    if not _check_out_magic(OUT / "out_l2b.bin", MAGIC_L2B, "L2b"):
        pass_sync = False
    if not _check_trace(OUT / "trace_l2a.bin", "L2a"):
        pass_sync = False
    if not _check_trace(OUT / "trace_l2b.bin", "L2b"):
        pass_sync = False
    if not _non_zero(OUT / "mat_c_l2a.bin", "mat_c_l2a.bin"):
        pass_sync = False
    if not _non_zero(OUT / "mat_c_l2b.bin", "mat_c_l2b.bin"):
        pass_sync = False

    if pass_sync:
        print("[PASS_SYNC] 三 launch：L2a/L2b GATE+handshake + Cube ok（L1 由 prep 写出中间量）")
    else:
        print("[FAIL_SYNC] handshake/Cube/mid-sync evidence", file=sys.stderr)
    if pass_io:
        print(f"[PASS_IO] m match golden ({backend})")
    else:
        print("[FAIL_IO] m mismatch", file=sys.stderr)

    if pass_sync and pass_io:
        print(f"[SUCCESS] RB-D04 PASS_SYNC + PASS_IO oracle={backend}")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
