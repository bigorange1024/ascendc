#!/usr/bin/env python3
"""
verify_result.py — 只检查 output/out.bin 的 magic 与长度。

约定（与 tiling.h / StubEncodeMagic 一致）：
  len == 64
  out[0:8] == b"SKELENC1"
  out[8]   == 0x15 若 SKEL_SKIPNTT=1 且 SKEL_HOST_MU=1（Host 折 μ）
           == 0x14 若 SKEL_SKIPNTT=1 且 SKEL_HOST_MU=0（设备 μ-stub）
           == 0x04 若 SKEL_GATE=1（且非 skipNtt）
           == 0xA5 若 GATE=0 且 SKIPNTT=0
  out[9:64] == b"\\xA5" * 55

不对 ML-KEM 算法正确性 / 数值 golden。
SKEL_* 读自环境变量（run.sh export），与编译宏一致。
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_PATH = os.path.join(ROOT, "output", "out.bin")

MAGIC_PREFIX = b"SKELENC1"
MAGIC_FILL = 0xA5
MAGIC_GATE_MARK = 0x04
MAGIC_SKIPNTT_MARK = 0x14  # 设备 μ-stub
MAGIC_HOST_MU_MARK = 0x15  # Host 折 μ
OUT_LEN = 64


def main() -> int:
    gate = os.environ.get("SKEL_GATE", "1").strip()
    if gate not in ("0", "1"):
        print(f"[FAIL] SKEL_GATE must be 0 or 1, got={gate!r}", file=sys.stderr)
        return 5
    heavy = os.environ.get("SKEL_HEAVY", "0").strip()
    if heavy not in ("0", "1"):
        print(f"[FAIL] SKEL_HEAVY must be 0 or 1, got={heavy!r}", file=sys.stderr)
        return 5
    skip = os.environ.get("SKEL_SKIPNTT", "0").strip()
    if skip not in ("0", "1"):
        print(f"[FAIL] SKEL_SKIPNTT must be 0 or 1, got={skip!r}", file=sys.stderr)
        return 5
    omit = os.environ.get("SKEL_OMIT_SET4", "0").strip()
    if omit not in ("0", "1"):
        print(f"[FAIL] SKEL_OMIT_SET4 must be 0 or 1, got={omit!r}", file=sys.stderr)
        return 5
    host_mu = os.environ.get("SKEL_HOST_MU", "1").strip()
    if host_mu not in ("0", "1"):
        print(f"[FAIL] SKEL_HOST_MU must be 0 or 1, got={host_mu!r}", file=sys.stderr)
        return 5

    if skip == "1":
        expect_b8 = MAGIC_HOST_MU_MARK if host_mu == "1" else MAGIC_SKIPNTT_MARK
    elif gate == "1":
        expect_b8 = MAGIC_GATE_MARK
    else:
        expect_b8 = MAGIC_FILL

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
    if data[8] != expect_b8:
        print(
            f"[FAIL] out[8]={data[8]:#04x} want={expect_b8:#04x} "
            f"(SKEL_SKIPNTT={skip}, SKEL_HOST_MU={host_mu}, SKEL_GATE={gate})",
            file=sys.stderr,
        )
        return 4
    if any(b != MAGIC_FILL for b in data[9:]):
        print("[FAIL] magic fill bytes[9:] mismatch (want 0xA5)", file=sys.stderr)
        return 4
    print(
        f"[SUCCESS] magic OK ({OUT_LEN} B, prefix={MAGIC_PREFIX!r}, "
        f"out[8]={data[8]:#04x}, SKEL_GATE={gate}, SKEL_HEAVY={heavy}, "
        f"SKEL_SKIPNTT={skip}, SKEL_OMIT_SET4={omit}, SKEL_HOST_MU={host_mu})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
