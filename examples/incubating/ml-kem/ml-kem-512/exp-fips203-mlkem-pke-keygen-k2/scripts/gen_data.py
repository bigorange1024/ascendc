#!/usr/bin/env python3
# @probe exp-fips203-mlkem-pke-keygen-k2
# @file scripts/gen_data.py
# @layer script
# @role 根级 gen_data：准备 seed/LUT 与可选 debug golden。 / Top-level input generator.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify run.sh 编译前生成 input/；KEYGEN_VERIFY=1 时参与对拍。

# coding=utf-8
"""
根级 gen_data：FIPS 203 Alg.13 KeyGen（ML-KEM-512，k=2）Host 侧输入/golden 生成入口。

## 流水线位置
`run.sh` 编译前调用：准备 `input/seed_d.bin` + LUT，并可选写 golden ek/dk。
本脚本**不**实现设备算法，仅编排 `keygen_golden.build_full_keygen`。

## 与 golden / 设备关系
验收仅 I/O 等价（ek_pke 800B、dk_pke 768B）；禁止把本 Host 路径当作 AscendC 规格。
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from keygen_golden import build_full_keygen, write_keygen_bins  # noqa: E402

# 默认种子：与 STATUS / KAT 常用值对齐；可用 SEED_D 覆盖
SEED_D_DEFAULT = 20260619


def write_golden_only(root: Path, kg: dict) -> None:
    """
    KEYGEN_GOLDEN_ONLY=1：仅写对拍用 golden，不改写 input/。

    参数:
        root: 用例根目录
        kg: build_full_keygen 返回的字典（须含 ek_pke / dk_pke ndarray）
    """
    out = root / "output"
    out.mkdir(exist_ok=True)
    kg["ek_pke"].tofile(out / "golden_ek_pke.bin")
    kg["dk_pke"].tofile(out / "golden_dk_pke.bin")


def main() -> None:
    """
    读环境变量 → 组装全链 golden → 写 input/ 与/或 output/golden_*。

    环境:
        SEED_D: uint32 种子（默认 SEED_D_DEFAULT）
        NTTS2S1E_MIX_PASS / TAG5T_MIX_PASS: 调试 mixPass（生产默认 0）
        KEYGEN_GOLDEN_ONLY=1: 只写 golden，不写 input
    """
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    # 生产默认 mixPass=0；非 0 仅调试分段，须显式 export
    mix_pass = int(os.environ.get("NTTS2S1E_MIX_PASS", os.environ.get("TAG5T_MIX_PASS", "0")))
    kg = build_full_keygen(seed_d, mix_pass=mix_pass)
    if os.environ.get("KEYGEN_GOLDEN_ONLY", "0") == "1":
        write_golden_only(ROOT, kg)
    else:
        # 写 seed + LUT（及调试中间 bin）；设备生产路径只消费 seed+LUT
        write_keygen_bins(ROOT, kg)
    print(
        f"[gen_data] SEED_D={seed_d} XOF_BYTES from alg7_geom mixPass={mix_pass} "
        f"ek_pke={len(kg['ek_pke'])}B dk_pke={len(kg['dk_pke'])}B"
    )


if __name__ == "__main__":
    main()
