#!/usr/bin/env python3
"""
verify_result.py — 只检查 output/out.bin 的 magic 与长度。

约定（与 tiling.h / StubEncodeMagic 一致）：
  len == 64
  out[0:8] == b"SKELDEC1"
  out[8]   == 0x04（合法 SoftSync+GATE 路径）
  out[9:64] == b"\\xA5" * 55

不对 ML-KEM 算法正确性 / 数值 golden。
OMIT_SET4=1 路径预期挂死，通常到不了本脚本（run.sh 在 timeout 124 退出）。
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_PATH = os.path.join(ROOT, "output", "out.bin")

MAGIC_PREFIX = b"SKELDEC1"
MAGIC_FILL = 0xA5
MAGIC_OK_MARK = 0x04
OUT_LEN = 64


def main() -> int:
    omit = os.environ.get("SKEL_OMIT_SET4", "0").strip()
    if omit not in ("0", "1"):
        print(f"[FAIL] SKEL_OMIT_SET4 must be 0 or 1, got={omit!r}", file=sys.stderr)
        return 5

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
    if data[8] != MAGIC_OK_MARK:
        print(
            f"[FAIL] out[8]={data[8]:#04x} want={MAGIC_OK_MARK:#04x} "
            f"(SKEL_OMIT_SET4={omit})",
            file=sys.stderr,
        )
        return 4
    if any(b != MAGIC_FILL for b in data[9:]):
        print("[FAIL] magic fill bytes[9:] mismatch (want 0xA5)", file=sys.stderr)
        return 4
    print(
        f"[SUCCESS] magic OK ({OUT_LEN} B, prefix={MAGIC_PREFIX!r}, "
        f"out[8]={data[8]:#04x}, SKEL_OMIT_SET4={omit})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
