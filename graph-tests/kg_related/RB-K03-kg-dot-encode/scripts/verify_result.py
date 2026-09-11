#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-K03 verify：PASS_SYNC（握手+Cube）+ PASS_IO（ek_pke/dk_pke 对拍；可选 t̂）。

硬条件：
  - out 魔数 K03:
  - TRACE Host PRE/POST；AIV 成对槽 GATE/DOT（勿硬绑 AIV0）
  - mat_c 非全 0
  - ek_pke / dk_pke 与 golden 逐字节一致（设备真算）

软：AIC TRACE、DONE 缺失仅告警；t_hat 对拍失败记 WARN（主验收仍为 ek/dk）。
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output"

MAGIC_OUT_OK = 0x4B30333A

HARD_HOST = {
    0: 0x484F5354,  # HOST_PRE
    11: 0x484F5355,  # HOST_POST_SYNC
}

HARD_AIV_PAIRS = [
    (1, 0xA1040004, 2, 0xA1140004, "PRE_SET4_GATE"),
    (3, 0xA1010001, 4, 0xA1110001, "PRE_SET1"),
    (8, 0xA1030003, 9, 0xA1130003, "POST_WAIT3"),
]

SOFT = {
    5: 0xC1040004,
    6: 0xC1010001,
    7: 0xC1030003,
    10: 0x4B30334F,  # DONE
}


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


def _cmp_bytes(got: Path, golden: Path, label: str) -> bool:
    if not got.is_file() or not golden.is_file():
        print(f"[FAIL] missing {label} or golden", file=sys.stderr)
        return False
    a = np.fromfile(got, dtype=np.uint8)
    b = np.fromfile(golden, dtype=np.uint8)
    if a.shape != b.shape:
        print(f"[FAIL] {label} shape {a.shape} != golden {b.shape}", file=sys.stderr)
        return False
    mism = int(np.count_nonzero(a != b))
    if mism != 0:
        # 找首错字节便于排错
        idx = int(np.flatnonzero(a != b)[0])
        print(
            f"[FAIL] {label} mism={mism}/{a.size} first@{idx} got={a[idx]} gold={b[idx]}",
            file=sys.stderr,
        )
        return False
    print(f"[OK] {label} max_abs=0 n={a.size}")
    return True


def _cmp_i32(got: Path, golden: Path, label: str) -> bool:
    if not got.is_file() or not golden.is_file():
        print(f"[WARN] missing {label} — skip", file=sys.stderr)
        return False
    a = np.fromfile(got, dtype=np.int32)
    b = np.fromfile(golden, dtype=np.int32)
    if a.shape != b.shape:
        print(f"[WARN] {label} shape {a.shape} != golden {b.shape}", file=sys.stderr)
        return False
    diff = int(np.max(np.abs(a.astype(np.int64) - b.astype(np.int64)))) if a.size else 0
    if diff != 0:
        mism = int(np.count_nonzero(a != b))
        print(f"[WARN] {label} max_abs={diff} mism={mism}/{a.size}", file=sys.stderr)
        return False
    print(f"[OK] {label} max_abs=0 n={a.size}")
    return True


def main() -> int:
    out_p = OUT / "out.bin"
    tr_p = OUT / "trace.bin"
    if not out_p.is_file() or not tr_p.is_file():
        print("[FAIL] missing output/out.bin or output/trace.bin", file=sys.stderr)
        return 1
    out = out_p.read_bytes()
    if len(out) < 4:
        print("[FAIL] out.bin too short", file=sys.stderr)
        return 1
    (out_magic,) = struct.unpack_from("<I", out, 0)
    if out_magic != MAGIC_OUT_OK:
        print(f"[FAIL] out magic 0x{out_magic:08X} != 0x{MAGIC_OUT_OK:08X}", file=sys.stderr)
        return 1

    tr = tr_p.read_bytes()
    if len(tr) < 12 * 4:
        print("[FAIL] trace.bin too short", file=sys.stderr)
        return 1
    vals = list(struct.unpack_from("<12I", tr, 0))

    pass_sync = True
    for slot, want in HARD_HOST.items():
        if vals[slot] != want:
            print(
                f"[FAIL] TRACE[{slot}]=0x{vals[slot]:08X} expect 0x{want:08X}",
                file=sys.stderr,
            )
            pass_sync = False
    for s0, m0, s1, m1, label in HARD_AIV_PAIRS:
        if not _check_aiv_pair(vals, s0, m0, s1, m1, label):
            pass_sync = False
    for slot, want in SOFT.items():
        if vals[slot] != want:
            print(f"[WARN] TRACE[{slot}]=0x{vals[slot]:08X} (soft expect 0x{want:08X})")
    if not _non_zero(OUT / "mat_c.bin", "mat_c.bin"):
        pass_sync = False

    pass_ek = _cmp_bytes(OUT / "ek_pke.bin", OUT / "golden_ek_pke.bin", "ek_pke")
    pass_dk = _cmp_bytes(OUT / "dk_pke.bin", OUT / "golden_dk_pke.bin", "dk_pke")
    _ = _cmp_i32(OUT / "t_hat.bin", OUT / "golden_t_hat.bin", "t_hat")
    pass_io = pass_ek and pass_dk

    if pass_sync:
        print("[PASS_SYNC] GATE(4)+DOT(1/3) causal + Cube ok")
    else:
        print("[FAIL_SYNC] handshake/Cube", file=sys.stderr)
    if pass_io:
        print("[PASS_IO] ek_pke+dk_pke match golden (device MIX)")
    else:
        print("[FAIL_IO] ek/dk mismatch", file=sys.stderr)

    if pass_sync and pass_io:
        print("[SUCCESS] RB-K03 PASS_SYNC + PASS_IO (max=0)")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
