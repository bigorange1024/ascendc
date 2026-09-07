#!/usr/bin/env python3
"""
verify_result.py — E01：校验 Host TRACE 轮次计数 + 可选 magic。

读 output/host_trace.log（run.sh tee）统计 111 次数；读 out.bin magic。
OMIT 路径不跑本脚本（预期 124）。
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "output")
TRACE_LOG = os.path.join(OUT, "host_trace.log")
OUT_BIN = os.path.join(OUT, "out.bin")

MAGIC = b"E01TOY01"
MARK = 0xE1


def main() -> int:
    rounds = int(os.environ.get("TOY_ROUNDS", "8"))
    if not os.path.isfile(TRACE_LOG):
        print(f"[FAIL] missing {TRACE_LOG}", file=sys.stderr)
        return 2
    with open(TRACE_LOG, "r", encoding="utf-8", errors="replace") as f:
        lines = [ln.strip() for ln in f if ln.strip()]
    # 仅统计纯三位数字行
    digits = [ln for ln in lines if ln.isdigit() and len(ln) == 3]
    c111 = sum(1 for d in digits if d == "111")
    c100 = sum(1 for d in digits if d == "100")
    c101 = sum(1 for d in digits if d == "101")
    c110 = sum(1 for d in digits if d == "110")
    print(f"[verify] host digits: 100={c100} 101={c101} 110={c110} 111={c111} (expect {rounds})")
    if c111 != rounds or c100 != rounds or c101 != rounds or c110 != rounds:
        print("[FAIL] Host TRACE round count mismatch", file=sys.stderr)
        last = digits[-1] if digits else "none"
        print(f"REPORT: C0 HANG last={last}", flush=True)
        return 3
    if not os.path.isfile(OUT_BIN):
        print(f"[FAIL] missing {OUT_BIN}", file=sys.stderr)
        print("REPORT: C0 FAIL missing_out", flush=True)
        return 4
    data = open(OUT_BIN, "rb").read()
    if len(data) < 9 or data[:8] != MAGIC or data[8] != MARK:
        print(f"[FAIL] magic mismatch: got {data[:9]!r}", file=sys.stderr)
        print("REPORT: C0 FAIL magic", flush=True)
        return 5
    print("[SUCCESS] E01 Host TRACE × rounds + magic OK")
    print("REPORT: C0 PASS last=111", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
