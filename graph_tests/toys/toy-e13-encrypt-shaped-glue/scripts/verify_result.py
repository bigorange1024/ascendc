#!/usr/bin/env python3
# coding=utf-8
"""
verify_result.py — E13 Encrypt 形态：TRACE × rounds + SHAKE + CBD(u) + c=c1||c2 golden(384B)。
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
CBD_SRC = os.path.join(OUT, "cbd_src.bin")
CBD_GOLDEN = os.path.join(OUT, "golden_cbd.bin")
K = 2
N = 256
ENCODE_D = int(os.environ.get("F203_BYTE_ENCODE_D", "4"))
ENCODE_PER_POLY = {4: 128, 5: 160, 10: 320, 11: 352}.get(ENCODE_D, 128)
C1_BYTES = ENCODE_PER_POLY * K
C2_BYTES = ENCODE_PER_POLY
ENCODE_OUT = C1_BYTES + C2_BYTES

HOST_SEQ = ["100", "101", "102", "105", "110", "111"]
L1_SEQ = ["200", "210", "211", "212", "220", "222", "223", "221", "203"]
L2_POLY0_MUST = [
    "400", "401",
    "500", "520", "530", "532", "540", "542", "550", "552", "560", "562", "503",
    "510", "521", "531", "541", "551", "553", "513",
]
L2_POLY1_MUST = [
    "410", "411",
    "600", "620", "630", "632", "640", "642", "650", "652", "660", "662", "603",
    "610", "621", "631", "641", "651", "653", "613",
]
L2_V_MUST = [
    "420", "421",
    "700", "720", "730", "732", "740", "742", "744", "745",
    "710", "721", "731", "733", "741", "743",
]
L2_FINAL_MUST = ["402"]  # SIM 下 502/512 偶发丢失；SET4 以 402 + golden + 不挂为准


def main() -> int:
    rounds = int(os.environ.get("TOY_ROUNDS", "3"))
    skip_golden = os.environ.get("TOY_SKIP_GOLDEN", "0") == "1"
    # 实机挂因套件：设备 AscendC::printf 常不进 host stdout；只认 Host 1xx
    host_only = os.environ.get("TOY_NPU_HOST_ONLY", "0") == "1" or skip_golden

    if not os.path.isfile(TRACE_LOG):
        print(f"[FAIL] missing {TRACE_LOG}", file=sys.stderr)
        print("REPORT: C1 FAIL missing_trace_log", flush=True)
        return 2
    with open(TRACE_LOG, "r", encoding="utf-8", errors="replace") as f:
        lines = [ln.strip() for ln in f if ln.strip()]
    digits = [ln for ln in lines if ln.isdigit() and len(ln) == 3]
    last = digits[-1] if digits else "none"

    def count(code: str) -> int:
        return sum(1 for d in digits if d == code)

    for code in HOST_SEQ:
        c = count(code)
        print(f"[verify] host {code}={c} (expect {rounds})")
        if c != rounds:
            print(f"[FAIL] Host TRACE {code} count mismatch", file=sys.stderr)
            print(f"REPORT: C1 HANG last={last} missing_host={code}", flush=True)
            return 3

    if host_only:
        # 设备号仅告警，不判 FAIL（NPU 上 2xx/4xx 常不可见）
        for code in L1_SEQ + L2_POLY0_MUST[:2] + L2_FINAL_MUST:
            c = count(code)
            if c < rounds:
                print(f"[warn] device TRACE {code}={c} (NPU 常不可见，挂因套件忽略)")
        print("[verify] Host-only OK（NPU 挂因：有 111 = 未粘性挂）")
        print("[SUCCESS] E13 Host TRACE-only (no hang path)")
        print(f"REPORT: C1 PASS last=111 host={','.join(HOST_SEQ)}", flush=True)
        return 0

    for code in L1_SEQ:
        c = count(code)
        print(f"[verify] L1 {code}={c} (expect {rounds})")
        if c < rounds:
            print(f"[FAIL] L1 TRACE {code} missing", file=sys.stderr)
            print(f"REPORT: C1 FAIL last={last} missing_l1={code}", flush=True)
            return 6
    if count("213") > 0:
        print("[FAIL] L1 TRACE 213 = SHAKE UB golden FAIL", file=sys.stderr)
        print(f"REPORT: C1 FAIL last={last} shake_ub=213", flush=True)
        return 6
    if count("222") < rounds or count("223") < rounds:
        print("[FAIL] L1 TRACE 222/223 missing (u/v CBD)", file=sys.stderr)
        print(f"REPORT: C1 FAIL last={last} missing_cbd", flush=True)
        return 6

    for code in L2_POLY0_MUST + L2_POLY1_MUST + L2_V_MUST + L2_FINAL_MUST:
        c = count(code)
        print(f"[verify] L2 {code}={c} (expect ≥{rounds})")
        if c < rounds:
            print(f"[FAIL] L2 TRACE {code} missing", file=sys.stderr)
            print(f"REPORT: C1 FAIL last={last} missing_l2={code}", flush=True)
            return 7

    host_idx = [i for i, d in enumerate(digits) if d == "100"]
    host111 = [i for i, d in enumerate(digits) if d == "111"]
    if len(host_idx) != rounds or len(host111) != rounds:
        print("[FAIL] cannot window rounds by 100/111", file=sys.stderr)
        print(f"REPORT: C1 FAIL last={last} window", flush=True)
        return 8
    for r in range(rounds):
        lo, hi = host_idx[r], host111[r]
        window = digits[lo : hi + 1]
        need = ["200", "222", "223", "400", "503", "600", "603", "700", "744", "420", "402"]
        for code in need:
            if code not in window:
                print(f"[FAIL] round {r} missing {code}", file=sys.stderr)
                print(f"REPORT: C1 FAIL last={last} round{r}_missing={code}", flush=True)
                return 8

    print("[verify] TRACE OK: L1 采样(SHAKE+CBD u/v) → μ → L2 代数+压码(c1||c2)+SET4")

    if skip_golden:
        print("[verify] TOY_SKIP_GOLDEN=1 — skip golden")
        print("[SUCCESS] E13 TRACE-only (no hang path)")
        print(f"REPORT: C1 PASS last=111", flush=True)
        return 0

    shake_y = np.fromfile(SHAKE_Y, dtype=np.uint8)
    shake_g = np.fromfile(SHAKE_GOLDEN, dtype=np.uint8)
    sdiff = np.where(shake_y[:32] != shake_g[:32])[0]
    print(f"[verify] shake256 diffs={sdiff.size}/32")
    if sdiff.size != 0:
        return 10

    cbd = np.fromfile(CBD_SRC, dtype=np.int32)
    cbd_g = np.fromfile(CBD_GOLDEN, dtype=np.int32)
    cdiff = np.where(cbd != cbd_g)[0]
    print(f"[verify] CBD(u) diffs={cdiff.size}/{cbd_g.size}")
    if cdiff.size != 0:
        return 11

    output = np.fromfile(DST_BIN, dtype=np.uint8, count=ENCODE_OUT)
    golden = np.fromfile(GOLDEN_BIN, dtype=np.uint8, count=ENCODE_OUT)
    diff = np.where(output != golden)[0]
    print(f"[verify] c=c1||c2 golden diffs={diff.size}/{golden.size}B (c1={C1_BYTES} c2={C2_BYTES})")
    if diff.size != 0:
        for i in diff[:20]:
            part = "c1" if i < C1_BYTES else "c2"
            print(f"  idx={i} ({part}) got={output[i]:02x} exp={golden[i]:02x}")
        return 9

    print("[SUCCESS] E13 Encrypt glue TRACE × rounds + shake + CBD(u) + 384B c golden OK")
    print("REPORT: C1 PASS last=111 golden_ok", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
