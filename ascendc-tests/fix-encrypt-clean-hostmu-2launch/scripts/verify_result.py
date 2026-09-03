#!/usr/bin/env python3
"""
verify_result.py — 只检查 output/out.bin 的 magic 与长度（P0，非 ML-KEM golden）。

约定：
  len == 64
  out[0:8] == b"CLNENC01"
  out[8]   == 0x21
  out[9:64] == b"\\xA5" * 55
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_PATH = os.path.join(ROOT, "output", "out.bin")

MAGIC_PREFIX = b"CLNENC01"
MAGIC_FILL = 0xA5
MAGIC_CLEAN_HOST_MU = 0x21
OUT_LEN = 64


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
    if data[8] != MAGIC_CLEAN_HOST_MU:
        print(
            f"[FAIL] out[8]={data[8]:#04x} want={MAGIC_CLEAN_HOST_MU:#04x} "
            f"(clean 2-launch Host-μ mark)",
            file=sys.stderr,
        )
        return 4
    if any(b != MAGIC_FILL for b in data[9:]):
        print("[FAIL] magic fill bytes[9:] mismatch (want 0xA5)", file=sys.stderr)
        return 4
    print(
        f"[SUCCESS] magic OK ({OUT_LEN} B, prefix={MAGIC_PREFIX!r}, "
        f"out[8]={data[8]:#04x}, HostFoldMuAlways + skipNtt no PrefixEmbed)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
