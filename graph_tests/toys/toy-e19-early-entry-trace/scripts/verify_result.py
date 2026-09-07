#!/usr/bin/env python3
"""
verify_result.py — E19：校验 Host TRACE 轮次 + 入口槽日志 + magic。

读 output/host_trace.log：统计 111、以及 `[e19-trace] … stages set=` 含 14 与 15。
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "output")
TRACE_LOG = os.path.join(OUT, "host_trace.log")
OUT_BIN = os.path.join(OUT, "out.bin")

MAGIC = b"E19TOY01"
MARK = 0xE9

# [e19-trace] round=N stages set=P/16 : 0 15
RE_TRACE = re.compile(
    r"\[e19-trace\]\s+round=(\d+)\s+stages\s+set=(\d+)/16\s+:(.*)$",
    re.M,
)


def main() -> int:
    rounds = int(os.environ.get("TOY_ROUNDS", "8"))
    if not os.path.isfile(TRACE_LOG):
        print(f"[FAIL] missing {TRACE_LOG}", file=sys.stderr)
        return 2
    with open(TRACE_LOG, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    digits = [ln for ln in lines if ln.isdigit() and len(ln) == 3]
    c111 = sum(1 for d in digits if d == "111")
    c100 = sum(1 for d in digits if d == "100")
    print(f"[verify] host digits: 100={c100} 111={c111} (expect {rounds})")
    if c111 != rounds or c100 != rounds:
        print("[FAIL] Host TRACE round count mismatch", file=sys.stderr)
        last = digits[-1] if digits else "none"
        print(f"REPORT: E19 HANG last={last}", flush=True)
        return 3

    entry_ok = 0
    for m in RE_TRACE.finditer(text):
        pop = int(m.group(2))
        slots = {int(x) for x in m.group(3).split() if x.isdigit()}
        if pop >= 2 and 0 in slots and 15 in slots:
            entry_ok += 1
    print(f"[verify] e19-trace entry rounds={entry_ok} (expect {rounds}; slots 0+15)")
    if entry_ok != rounds:
        print("[FAIL] entry slots 0/15 not seen each round", file=sys.stderr)
        print("REPORT: E19 FAIL entry_slots", flush=True)
        return 6

    if not os.path.isfile(OUT_BIN):
        print(f"[FAIL] missing {OUT_BIN}", file=sys.stderr)
        print("REPORT: E19 FAIL missing_out", flush=True)
        return 4
    data = open(OUT_BIN, "rb").read()
    if len(data) < 9 or data[:8] != MAGIC or data[8] != MARK:
        print(f"[FAIL] magic mismatch: got {data[:9]!r}", file=sys.stderr)
        print("REPORT: E19 FAIL magic", flush=True)
        return 5
    print("[SUCCESS] E19 Host TRACE × rounds + entry slots 0/15 + magic OK")
    print("REPORT: E19 PASS last=111 entry=0,15", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
