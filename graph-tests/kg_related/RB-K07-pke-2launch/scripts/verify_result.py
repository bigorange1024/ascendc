#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""RB-K07 verify：二 launch 融合 MIX 验证 —— ek/dk ≡ liboqs（I/O 等价）+ 段1/段2 TRACE 因果证据。

背景：RB-K07 把 RB-K04 的 3 launch（prep → L2a NTT → L2b 点积/编码）压缩为 2 launch
（prep → 单 MIX 核内串行两段握手）。本脚本不再检查独立的 L2a/L2b 产物文件，而是检查融合核
产出的 `out.bin`（单一魔数，K03 语义）、`trace_seg1.bin`（段1 NTT 握手）、`trace_seg2.bin`
（段2 点积/编码握手）、`mat_c_seg1.bin`/`mat_c_seg2.bin`（两段各自的 Cube 握手矩阵非全零证据）。
"""
from __future__ import annotations
import struct
import sys
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output"
# 融合核唯一 out 落点使用 K03（段2/点积编码）语义的成功魔数（见 k03_inc/tiling.h MAGIC_OUT_OK）。
MAGIC_OUT_OK = 0x4B30333A
HARD_AIV_PAIRS = [
    (1, 0xA1040004, 2, 0xA1140004, "PRE_SET4_GATE"),
    (3, 0xA1010001, 4, 0xA1110001, "PRE_SET1"),
    (8, 0xA1030003, 9, 0xA1130003, "POST_WAIT3"),
]


def _check_aiv_pair(vals, s0, m0, s1, m1, label):
    g0, g1 = vals[s0], vals[s1]
    ok0, ok1 = g0 == m0, g1 == m1
    if not ok0 and not ok1:
        print(f"[FAIL] TRACE {label}: AIV0[{s0}]=0x{g0:08X} AIV1[{s1}]=0x{g1:08X}", file=sys.stderr)
        return False
    if not ok0:
        print(f"[WARN] TRACE[{s0}] AIV0 {label} empty; AIV1 ok")
    if not ok1:
        print(f"[WARN] TRACE[{s1}] AIV1 {label} empty; AIV0 ok")
    return True


def _non_zero(path, label):
    if not path.is_file():
        print(f"[FAIL] missing {label}", file=sys.stderr)
        return False
    data = path.read_bytes()
    if len(data) < 4 or all(b == 0 for b in data):
        print(f"[FAIL] {label} all-zero", file=sys.stderr)
        return False
    return True


def _check_trace_aiv(path, tag):
    if not path.is_file():
        print(f"[FAIL] missing {path.name}", file=sys.stderr)
        return False
    tr = path.read_bytes()
    if len(tr) < 48:
        print(f"[FAIL] {path.name} too short", file=sys.stderr)
        return False
    vals = list(struct.unpack_from("<12I", tr, 0))
    ok = True
    for s0, m0, s1, m1, label in HARD_AIV_PAIRS:
        if not _check_aiv_pair(vals, s0, m0, s1, m1, f"{tag}:{label}"):
            ok = False
    return ok


def _check_out_magic(path, want, tag):
    if not path.is_file():
        print(f"[FAIL] missing {path.name}", file=sys.stderr)
        return False
    data = path.read_bytes()
    if len(data) < 4:
        print(f"[FAIL] {path.name} too short", file=sys.stderr)
        return False
    (got,) = struct.unpack_from("<I", data, 0)
    if got != want:
        print(f"[FAIL] {tag} out magic 0x{got:08X} != 0x{want:08X}", file=sys.stderr)
        return False
    print(f"[OK] {tag} out magic 0x{got:08X}")
    return True


def _cmp_bytes(got, golden, label):
    if not got.is_file() or not golden.is_file():
        print(f"[FAIL] missing {label}", file=sys.stderr)
        return False
    a = np.fromfile(got, dtype=np.uint8)
    b = np.fromfile(golden, dtype=np.uint8)
    if a.shape != b.shape or int(np.count_nonzero(a != b)) != 0:
        print(f"[FAIL] {label} mismatch", file=sys.stderr)
        return False
    print(f"[OK] {label} max_abs=0 n={a.size}")
    return True


def main():
    backend = "unknown"
    bp = OUT / "cross_backend.txt"
    if bp.is_file() and bp.stat().st_size:
        backend = bp.read_text(encoding="utf-8").strip().splitlines()[0]
    pass_io = _cmp_bytes(OUT / "ek_pke.bin", OUT / "golden_ek_pke.bin", "ek_pke") and \
              _cmp_bytes(OUT / "dk_pke.bin", OUT / "golden_dk_pke.bin", "dk_pke")
    if pass_io:
        print(f"[OK] ek/dk max=0 oracle={backend}")
    pass_sync = True
    if not _check_out_magic(OUT / "out.bin", MAGIC_OUT_OK, "fused"):
        pass_sync = False
    if not _check_trace_aiv(OUT / "trace_seg1.bin", "seg1"):
        pass_sync = False
    if not _check_trace_aiv(OUT / "trace_seg2.bin", "seg2"):
        pass_sync = False
    if not _non_zero(OUT / "mat_c_seg1.bin", "mat_c_seg1.bin"):
        pass_sync = False
    if not _non_zero(OUT / "mat_c_seg2.bin", "mat_c_seg2.bin"):
        pass_sync = False
    if pass_sync:
        print("[PASS_SYNC] 2-launch fused MIX seg1+seg2 evidence")
    else:
        print("[FAIL_SYNC]", file=sys.stderr)
    if pass_io:
        print(f"[PASS_IO] ek/dk match ({backend})")
    else:
        print("[FAIL_IO]", file=sys.stderr)
    if pass_sync and pass_io:
        print(f"[SUCCESS] RB-K07 PASS_SYNC + PASS_IO oracle={backend}")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
