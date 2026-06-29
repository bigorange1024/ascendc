#!/usr/bin/env python3
# @probe stable-mlkem-f203-pke-keygen-k4
# @file scripts/gen_data.py
# @layer script
# @role 根级 gen_data：准备 seed/LUT 与可选 debug golden。 / Top-level input generator.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify run.sh 编译前生成 input/；KEYGEN_VERIFY=1 时参与对拍。

# coding=utf-8
"""G0：生成 Alg.13 全链 golden（仅 Host）。"""
from __future__ import annotations

import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from keygen_golden import build_full_keygen, write_keygen_bins  # noqa: E402

SEED_D_DEFAULT = 20260619


def write_golden_only(root: Path, kg: dict) -> None:
    """KEYGEN_GOLDEN_ONLY=1：仅写 output/golden_ek_pke.bin 与 golden_dk_pke.bin，不污染 input/。"""
    out = root / "output"
    out.mkdir(exist_ok=True)
    kg["ek_pke"].tofile(out / "golden_ek_pke.bin")
    kg["dk_pke"].tofile(out / "golden_dk_pke.bin")


def main() -> None:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    mix_pass = int(os.environ.get("NTTS2S1E_MIX_PASS", os.environ.get("TAG5T_MIX_PASS", "0")))
    kg = build_full_keygen(seed_d, mix_pass=mix_pass)
    if os.environ.get("KEYGEN_GOLDEN_ONLY", "0") == "1":
        write_golden_only(ROOT, kg)
    else:
        write_keygen_bins(ROOT, kg)
    print(
        f"[gen_data] SEED_D={seed_d} XOF_BYTES from alg7_geom mixPass={mix_pass} "
        f"ek_pke={len(kg['ek_pke'])}B dk_pke={len(kg['dk_pke'])}B"
    )


if __name__ == "__main__":
    main()
