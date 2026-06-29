#!/usr/bin/env python3
# @probe stable-mlkem-f203-pke-keygen-k4
# @file scripts/verify_production.py
# @layer script
# @role 对拍 output/ek_pke+dk_pke 与 golden（生产验收）。 / Production verify.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify python3 调用或由 run.sh 对拍 output vs golden。

# coding=utf-8
"""生产路径对拍：仅 ek_PKE / dk_PKE 与 golden 字节一致。"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "output"


def main() -> int:
    for name, gold in (("ek_pke.bin", "golden_ek_pke.bin"), ("dk_pke.bin", "golden_dk_pke.bin")):
        got_p = OUT / name
        gold_p = OUT / gold
        if not got_p.is_file():
            raise SystemExit(f"missing output/{name}")
        if not gold_p.is_file():
            raise SystemExit(f"missing output/{gold} (run KEYGEN_GOLDEN_ONLY=1 gen_data.py first)")
        got = np.fromfile(got_p, dtype=np.uint8)
        exp = np.fromfile(gold_p, dtype=np.uint8)
        if got.shape != exp.shape or not np.array_equal(got, exp):
            raise SystemExit(f"[verify] FAIL {name}")
        print(f"[verify] {name} PASS (bytes={got.size})")
    print("[verify] production overall PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
