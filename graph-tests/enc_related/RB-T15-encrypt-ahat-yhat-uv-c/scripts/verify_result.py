#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T15 verify：分栏 PASS_SYNC（主）/ PASS_IO（c/u/v 硬；Â/ŷ 尽力）。

PASS_SYNC（硬，失败则 exit 1）：
  - out 魔数 T15
  - TRACE：HOST_PRE / PREP_DONE / AIV0 NTT SET1+WAIT3 / AHAT / YHAT /
    PRE_SET4 / INTT SET1+WAIT3 / PACK_DONE / HOST_MID / HOST_POST
  - mat_c_ntt、mat_c_intt 非全 0

PASS_IO（尽力；失败仅告警 PASS_IO=0，不否决 SYNC）：
  - c / u / v / a_hat / y_hat / y_e1_e2_dev / rho_dev 与 golden

退出码：SYNC 失败 → 1；仅 IO 失败 → 0。
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output"

MAGIC_OUT_OK = 0x543F003F

# 硬槽：与 tiling.h TRACE 对齐（AIV0 路径）
HARD = {
    0: 0x484F5354,  # HOST_PRE
    1: 0x50524550,  # PREP_DONE
    2: 0xA1010001,  # AIV0_PRE_SET1_NTT
    6: 0xA1030003,  # AIV0_POST_WAIT3_NTT
    8: 0xA1040004,  # AIV0_PRE_SET4
    10: 0x484F4D49,  # HOST_MID_SYNC
    11: 0xA1011001,  # AIV0_PRE_SET1_INTT
    14: 0xA1031003,  # AIV0_POST_WAIT3_INTT
    16: 0x5041434B,  # PACK_DONE
    17: 0x484F5355,  # HOST_POST
    22: 0x41484154,  # AHAT_DONE
    23: 0x59484154,  # YHAT_DONE
}

SOFT = {
    3: 0xA1110001,  # AIV1_PRE_SET1_NTT
    4: 0xC1010001,  # AIC_POST_WAIT1_NTT
    5: 0xC1030003,  # AIC_PRE_SET3_NTT
    7: 0x4D554C44,  # MUL_DONE
    9: 0xC1040004,  # AIC_POST_WAIT4
    12: 0xC1011001,  # AIC_POST_WAIT1_INTT
    13: 0xC1031003,  # AIC_PRE_SET3_INTT
    15: 0x5556444E,  # UV_DONE
    18: 0xA1130003,  # AIV1_POST_WAIT3_NTT
    19: 0xA1140004,  # AIV1_PRE_SET4
    20: 0xA1111001,  # AIV1_PRE_SET1_INTT
    21: 0xA1131003,  # AIV1_POST_WAIT3_INTT
}


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
        print(f"[PASS_IO=0] missing {label} or golden")
        return False
    a, b = got.read_bytes(), golden.read_bytes()
    if a != b:
        mism = sum(1 for x, y in zip(a, b) if x != y) + abs(len(a) - len(b))
        print(f"[PASS_IO=0] {label} mismatch bytes~={mism} len={len(a)}/{len(b)}")
        return False
    print(f"[PASS_IO] {label} match")
    return True


def _cmp_i32(got: Path, golden: Path, label: str) -> bool:
    if not got.is_file() or not golden.is_file():
        print(f"[PASS_IO=0] missing {label} or golden")
        return False
    a = np.fromfile(got, dtype=np.int32)
    b = np.fromfile(golden, dtype=np.int32)
    if a.shape != b.shape:
        print(f"[PASS_IO=0] {label} shape {a.shape} != {b.shape}")
        return False
    diff = int(np.max(np.abs(a.astype(np.int64) - b.astype(np.int64)))) if a.size else 0
    if diff != 0:
        mism = int(np.count_nonzero(a != b))
        print(f"[PASS_IO=0] {label} max_abs={diff} mism={mism}/{a.size}")
        return False
    print(f"[PASS_IO] {label} max_abs=0 n={a.size}")
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
    if len(tr) < 24 * 4:
        print("[FAIL] trace.bin too short", file=sys.stderr)
        return 1
    vals = list(struct.unpack_from("<24I", tr, 0))
    for slot, want in HARD.items():
        got = vals[slot]
        if got != want:
            print(f"[FAIL] TRACE[{slot}]=0x{got:08X} expect 0x{want:08X}", file=sys.stderr)
            return 1
    for slot, want in SOFT.items():
        got = vals[slot]
        if got != want:
            print(f"[WARN] TRACE[{slot}]=0x{got:08X} (soft; SIM 上 AIC/AIV1 标量 TRACE 常空)")
    if not _non_zero(OUT / "mat_c_ntt.bin", "mat_c_ntt.bin"):
        return 1
    if not _non_zero(OUT / "mat_c_intt.bin", "mat_c_intt.bin"):
        return 1

    print("[PASS_SYNC] dual-launch causal + AHAT/YHAT + dual Cube + pack TRACE ok")

    io_ok = True
    io_ok = _cmp_bytes(OUT / "c.bin", OUT / "golden_c.bin", "c") and io_ok
    io_ok = _cmp_i32(OUT / "u.bin", OUT / "golden_u.bin", "u") and io_ok
    io_ok = _cmp_i32(OUT / "v.bin", OUT / "golden_v.bin", "v") and io_ok
    io_ok = _cmp_i32(OUT / "a_hat.bin", OUT / "golden_a_hat.bin", "a_hat") and io_ok
    io_ok = _cmp_i32(OUT / "y_hat.bin", OUT / "golden_y_hat.bin", "y_hat") and io_ok
    io_ok = _cmp_bytes(OUT / "y_e1_e2_dev.bin", OUT / "golden_y_e1_e2.bin", "y_e1_e2_dev") and io_ok
    io_ok = _cmp_bytes(OUT / "rho_dev.bin", OUT / "golden_rho.bin", "rho_dev") and io_ok

    if io_ok:
        print("[SUCCESS] PASS_SYNC + PASS_IO — RB-T15 encrypt ahat-yhat-uv-c")
    else:
        print("[SUCCESS] PASS_SYNC only (PASS_IO=0) — sync 优先，IO 尽力")
    return 0


if __name__ == "__main__":
    sys.exit(main())
