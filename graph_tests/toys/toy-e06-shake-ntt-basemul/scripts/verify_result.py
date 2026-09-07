#!/usr/bin/env python3
# coding=utf-8
"""
verify_result.py — E06：TRACE × rounds + SHAKE 短向量 + MultiplyNTTs(basemul) golden。

读 output/host_trace.log、shake_y.bin、dst.bin / golden.bin。
basemul golden = Alg.11/12(NTT(src), ĝ)；NTT ≠ Tag5T。
若 TOY_SKIP_GOLDEN=1，则只验 TRACE（不挂优先）。
"""
import os
import sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "output")
TRACE_LOG = os.path.join(OUT, "host_trace.log")
DST_BIN = os.path.join(OUT, "dst.bin")
GOLDEN_BIN = os.path.join(OUT, "golden.bin")
SHAKE_Y = os.path.join(OUT, "shake_y.bin")
SHAKE_GOLDEN = os.path.join(OUT, "shake_golden.bin")

HOST_SEQ = ["100", "101", "105", "110", "111"]
L1_SEQ = ["200", "210", "211", "212", "203"]
# 530/531=真 basemul 开始；532/533=完成；540/541=假 INTT TRACE
L2_MUST = [
    "400", "401", "402",
    "500", "520", "530", "532", "540", "502",
    "510", "521", "531", "533", "541", "512",
]


def main() -> int:
    rounds = int(os.environ.get("TOY_ROUNDS", "3"))
    skip_golden = os.environ.get("TOY_SKIP_GOLDEN", "0") == "1"

    if not os.path.isfile(TRACE_LOG):
        print(f"[FAIL] missing {TRACE_LOG}", file=sys.stderr)
        return 2
    with open(TRACE_LOG, "r", encoding="utf-8", errors="replace") as f:
        lines = [ln.strip() for ln in f if ln.strip()]
    digits = [ln for ln in lines if ln.isdigit() and len(ln) == 3]

    def count(code: str) -> int:
        return sum(1 for d in digits if d == code)

    for code in HOST_SEQ:
        c = count(code)
        print(f"[verify] host {code}={c} (expect {rounds})")
        if c != rounds:
            print(f"[FAIL] Host TRACE {code} count mismatch", file=sys.stderr)
            return 3

    host_only = [d for d in digits if d in HOST_SEQ]
    if len(host_only) != rounds * len(HOST_SEQ):
        print(f"[FAIL] host digit stream length {len(host_only)}", file=sys.stderr)
        return 3
    for r in range(rounds):
        chunk = host_only[r * len(HOST_SEQ) : (r + 1) * len(HOST_SEQ)]
        if chunk != HOST_SEQ:
            print(f"[FAIL] host order round {r}: {chunk}", file=sys.stderr)
            return 3

    for code in L1_SEQ:
        c = count(code)
        print(f"[verify] L1 {code}={c} (expect {rounds})")
        if c < rounds:
            print(f"[FAIL] L1 TRACE {code} missing (need real SHAKE path)", file=sys.stderr)
            return 6
    if count("213") > 0:
        print("[FAIL] L1 TRACE 213 = SHAKE UB golden FAIL", file=sys.stderr)
        return 6

    for code in L2_MUST:
        c = count(code)
        print(f"[verify] L2 {code}={c} (expect ≥{rounds})")
        if c < rounds:
            print(f"[FAIL] L2 TRACE {code} missing", file=sys.stderr)
            return 7

    host_idx = [i for i, d in enumerate(digits) if d == "100"]
    host111 = [i for i, d in enumerate(digits) if d == "111"]
    if len(host_idx) != rounds or len(host111) != rounds:
        print("[FAIL] cannot window rounds by 100/111", file=sys.stderr)
        return 8
    for r in range(rounds):
        lo, hi = host_idx[r], host111[r]
        window = digits[lo : hi + 1]
        if "200" not in window or "400" not in window:
            print(f"[FAIL] round {r} missing 200/400", file=sys.stderr)
            return 8
        if window.index("200") > window.index("400"):
            print(f"[FAIL] round {r}: 200 after 400", file=sys.stderr)
            return 8
        for a, b in zip(L1_SEQ, L1_SEQ[1:]):
            if window.index(a) > window.index(b):
                print(f"[FAIL] round {r}: L1 {a} after {b}", file=sys.stderr)
                return 8
        aiv0 = ["500", "520", "530", "532", "540", "502"]
        for a, b in zip(aiv0, aiv0[1:]):
            if a not in window or b not in window or window.index(a) > window.index(b):
                print(f"[FAIL] round {r}: AIV0 order {a}->{b}", file=sys.stderr)
                return 8

    print("[verify] TRACE OK: L1 real-SHAKE → μ → L2 real-NTT + real-basemul + SET4")

    if skip_golden:
        print("[verify] TOY_SKIP_GOLDEN=1 — skip shake/basemul golden")
        print("[SUCCESS] E06 TRACE-only (no hang path)")
        return 0

    if not os.path.isfile(SHAKE_Y) or not os.path.isfile(SHAKE_GOLDEN):
        print(f"[FAIL] missing shake_y/shake_golden under {OUT}", file=sys.stderr)
        return 4
    shake_y = np.fromfile(SHAKE_Y, dtype=np.uint8)
    shake_g = np.fromfile(SHAKE_GOLDEN, dtype=np.uint8)
    if shake_y.size < 32 or shake_g.size < 32:
        print(f"[FAIL] shake size y={shake_y.size} g={shake_g.size}", file=sys.stderr)
        return 5
    sdiff = np.where(shake_y[:32] != shake_g[:32])[0]
    print(f"[verify] shake256(abc→32B) diffs={sdiff.size}/32")
    if sdiff.size != 0:
        for i in sdiff[:16]:
            print(f"  shake idx={i} got={shake_y[i]:02x} exp={shake_g[i]:02x}")
        print("[FAIL] shake short-vector mismatch", file=sys.stderr)
        return 10

    if not os.path.isfile(DST_BIN) or not os.path.isfile(GOLDEN_BIN):
        print(f"[FAIL] missing dst/golden under {OUT}", file=sys.stderr)
        return 4
    output = np.fromfile(DST_BIN, dtype=np.int32).reshape(-1)
    golden = np.fromfile(GOLDEN_BIN, dtype=np.int32).reshape(-1)
    if output.size != golden.size:
        print(f"[FAIL] size mismatch out={output.size} golden={golden.size}", file=sys.stderr)
        return 5
    diff = np.where(output != golden)[0]
    print(f"[verify] MultiplyNTTs/basemul golden diffs={diff.size}/{golden.size}")
    if diff.size != 0:
        for i in diff[:20]:
            print(f"  idx={i} got={output[i]} exp={golden[i]}")
        print("[FAIL] basemul golden mismatch", file=sys.stderr)
        return 9
    print("[SUCCESS] E06 TRACE × rounds + shake + MultiplyNTTs basemul golden OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
