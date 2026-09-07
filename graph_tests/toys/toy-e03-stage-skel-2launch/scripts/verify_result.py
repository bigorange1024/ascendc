#!/usr/bin/env python3
"""
verify_result.py — E03：校验 Host TRACE 轮次 + 采样→代数阶段顺序 + magic。

读 output/host_trace.log（run.sh tee）统计轮次与阶段号段；读 out.bin magic。
设备侧 printf 在 SIM 下常混入同一 tee，用于确认 200–203 出现在 L2 代数号段之前。
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "output")
TRACE_LOG = os.path.join(OUT, "host_trace.log")
OUT_BIN = os.path.join(OUT, "out.bin")

MAGIC = b"E03TOY01"
MARK = 0xE3

# 每轮期望的 Host 骨架顺序（不含设备号）
HOST_SEQ = ["100", "101", "105", "110", "111"]
# L1 采样 stub（设备）
L1_SEQ = ["200", "201", "202", "203"]
# L2 代数关键点（至少出现；AIC Wait + AIV0 假代数 + SET）
L2_MUST = ["400", "401", "402", "500", "520", "530", "540", "502", "510", "521", "531", "541", "512"]


def main() -> int:
    rounds = int(os.environ.get("TOY_ROUNDS", "3"))
    if not os.path.isfile(TRACE_LOG):
        print(f"[FAIL] missing {TRACE_LOG}", file=sys.stderr)
        return 2
    with open(TRACE_LOG, "r", encoding="utf-8", errors="replace") as f:
        lines = [ln.strip() for ln in f if ln.strip()]
    digits = [ln for ln in lines if ln.isdigit() and len(ln) == 3]

    def count(code: str) -> int:
        return sum(1 for d in digits if d == code)

    # Host 轮次
    for code in HOST_SEQ:
        c = count(code)
        print(f"[verify] host {code}={c} (expect {rounds})")
        if c != rounds:
            print(f"[FAIL] Host TRACE {code} count mismatch", file=sys.stderr)
            return 3

    # Host 顺序：每轮 100→101→105→110→111
    host_only = [d for d in digits if d in HOST_SEQ]
    if len(host_only) != rounds * len(HOST_SEQ):
        print(f"[FAIL] host digit stream length {len(host_only)}", file=sys.stderr)
        return 3
    for r in range(rounds):
        chunk = host_only[r * len(HOST_SEQ) : (r + 1) * len(HOST_SEQ)]
        if chunk != HOST_SEQ:
            print(f"[FAIL] host order round {r}: {chunk}", file=sys.stderr)
            return 3

    # L1 采样 stub 计数
    for code in L1_SEQ:
        c = count(code)
        print(f"[verify] L1 {code}={c} (expect {rounds})")
        if c < rounds:
            print(f"[FAIL] L1 TRACE {code} missing/insufficient", file=sys.stderr)
            return 6

    # L2 代数 / SET4 关键点
    for code in L2_MUST:
        c = count(code)
        print(f"[verify] L2 {code}={c} (expect ≥{rounds})")
        if c < rounds:
            print(f"[FAIL] L2 TRACE {code} missing/insufficient", file=sys.stderr)
            return 7

    # 阶段顺序：同一轮内首次 200 须早于首次 400（相对该轮 Host 100..111 窗口）
    # 简化：全局首次 200 索引 < 首次 400；且末次 203 前应已有完整 L1
    try:
        i200 = digits.index("200")
        i400 = digits.index("400")
    except ValueError:
        print("[FAIL] cannot find 200 or 400 in digit stream", file=sys.stderr)
        return 8
    if not (i200 < i400):
        print(f"[FAIL] stage order: first 200@{i200} not before first 400@{i400}", file=sys.stderr)
        return 8
    # 每轮：在相邻 Host 100..111 之间，200 应出现在 400 之前
    # 用 Host 标记切窗
    host_idx = [i for i, d in enumerate(digits) if d == "100"]
    host111 = [i for i, d in enumerate(digits) if d == "111"]
    if len(host_idx) != rounds or len(host111) != rounds:
        print("[FAIL] cannot window rounds by 100/111", file=sys.stderr)
        return 8
    for r in range(rounds):
        lo, hi = host_idx[r], host111[r]
        window = digits[lo : hi + 1]
        if "200" not in window or "400" not in window:
            print(f"[FAIL] round {r} window missing 200/400: {window}", file=sys.stderr)
            return 8
        if window.index("200") > window.index("400"):
            print(f"[FAIL] round {r}: 200 after 400 in window", file=sys.stderr)
            return 8
        # L1 内部顺序
        for a, b in zip(L1_SEQ, L1_SEQ[1:]):
            if window.index(a) > window.index(b):
                print(f"[FAIL] round {r}: L1 order {a} after {b}", file=sys.stderr)
                return 8
        # AIV0 假代数顺序 500→520→530→540→502
        aiv0 = ["500", "520", "530", "540", "502"]
        for a, b in zip(aiv0, aiv0[1:]):
            if a not in window or b not in window or window.index(a) > window.index(b):
                print(f"[FAIL] round {r}: AIV0 algebra order {a}->{b}", file=sys.stderr)
                return 8

    print("[verify] stage order OK: L1 sampling → Host μ → L2 algebra+SET4")

    if not os.path.isfile(OUT_BIN):
        print(f"[FAIL] missing {OUT_BIN}", file=sys.stderr)
        return 4
    data = open(OUT_BIN, "rb").read()
    if len(data) < 9 or data[:8] != MAGIC or data[8] != MARK:
        print(f"[FAIL] magic mismatch: got {data[:9]!r}", file=sys.stderr)
        return 5
    print("[SUCCESS] E03 Host TRACE × rounds + stage TRACE + magic OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
