#!/usr/bin/env python3
"""
verify_result.py — P1a：magic + L2 TRACE 非空（非 ML-KEM golden）。

约定：
  out.bin：len==64；prefix CLNENC01；out[8]==0x2A；fill 0xA5
  trace.bin：6×int32；至少 AIV 槽 0,1,2 非零（SET(4) 前可达可达）
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_PATH = os.path.join(ROOT, "output", "out.bin")
TRACE_PATH = os.path.join(ROOT, "output", "trace.bin")

MAGIC_PREFIX = b"CLNENC01"
MAGIC_FILL = 0xA5
MAGIC_P1A = 0x2A
OUT_LEN = 64
TRACE_STAGES = 6
# P1a 最低要求：SET(4) 前两条 + SET(4) 后一条（服务空 TRACE）
REQUIRED_AIV = (0, 1, 2)


def main() -> int:
    if not os.path.isfile(OUT_PATH):
        print(f"[FAIL] missing {OUT_PATH}", file=sys.stderr)
        return 1
    data = open(OUT_PATH, "rb").read()
    if len(data) != OUT_LEN:
        print(f"[FAIL] out len={len(data)} want={OUT_LEN}", file=sys.stderr)
        return 2
    if data[:8] != MAGIC_PREFIX:
        print(f"[FAIL] magic prefix={data[:8]!r} want={MAGIC_PREFIX!r}", file=sys.stderr)
        return 3
    if data[8] != MAGIC_P1A:
        print(
            f"[FAIL] out[8]={data[8]:#04x} want={MAGIC_P1A:#04x} (P1a TRACE mark)",
            file=sys.stderr,
        )
        return 4
    if any(b != MAGIC_FILL for b in data[9:]):
        print("[FAIL] magic fill bytes[9:] mismatch (want 0xA5)", file=sys.stderr)
        return 4

    if not os.path.isfile(TRACE_PATH):
        print(f"[FAIL] missing {TRACE_PATH} (P1a)", file=sys.stderr)
        return 5
    raw = open(TRACE_PATH, "rb").read()
    if len(raw) != TRACE_STAGES * 4:
        print(f"[FAIL] trace len={len(raw)} want={TRACE_STAGES * 4}", file=sys.stderr)
        return 6
    stages = struct.unpack(f"<{TRACE_STAGES}i", raw)
    missing = [i for i in REQUIRED_AIV if stages[i] == 0]
    if missing:
        print(f"[FAIL] P1a TRACE missing required stages {missing}; got {stages}", file=sys.stderr)
        return 7
    pop = sum(1 for s in stages if s != 0)
    print(
        f"[SUCCESS] magic+TRACE OK ({OUT_LEN} B, out[8]={data[8]:#04x}, "
        f"trace set={pop}/{TRACE_STAGES} stages={list(stages)})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
