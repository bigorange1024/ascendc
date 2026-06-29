#!/usr/bin/env python3
# @probe pass-fix-f203-alg13-device-keygen-k4
# @file scripts/prepare_production_input.py
# @layer script
# @role 从 seed 生成生产 input/（seed_d + stacked LUT）。 / Production input prep.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。

# coding=utf-8
"""生产 I/O：仅准备 input/seed_d.bin；LUT 缺失时从本目录 compute golden 生成。

契约（与 exp customspec 一致）：
- input/：SEED_D + 静态 NTT LUT（lut_even/odd_stacked.bin）
- 禁止向 input/ 写入 a_hat、src、rho、tiling 等中间态
"""
from __future__ import annotations

import importlib.util
import os
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
_COMPUTE_GEN = ROOT / "scripts" / "compute" / "gen_data.py"

_spec = importlib.util.spec_from_file_location("compute_gen_data", _COMPUTE_GEN)
compute_gen = importlib.util.module_from_spec(_spec)
assert _spec.loader is not None
_spec.loader.exec_module(compute_gen)

_STRAY_INPUT = (
    "a_hat.bin",
    "src.bin",
    "rho.bin",
    "ek_polyvec.bin",
    "tiling.bin",
)


def write_lut_if_missing(inp: Path) -> None:
    lut_even = inp / "lut_even_stacked.bin"
    lut_odd = inp / "lut_odd_stacked.bin"
    if lut_even.is_file() and lut_odd.is_file():
        return
    lut = compute_gen.load_lut_t_i8()
    compute_gen.lut_planar_stacked(lut, even=True).tofile(lut_even)
    compute_gen.lut_planar_stacked(lut, even=False).tofile(lut_odd)
    print(f"[prepare_input] wrote LUT -> {lut_even.name}, {lut_odd.name}")


def scrub_stray_input(inp: Path) -> None:
    for name in _STRAY_INPUT:
        p = inp / name
        if p.is_file():
            p.unlink()
            print(f"[prepare_input] removed stray input/{name}")


def main() -> None:
    seed_d = int(os.environ.get("SEED_D", "20260619"))
    inp = ROOT / "input"
    inp.mkdir(exist_ok=True)
    (inp / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
    write_lut_if_missing(inp)
    scrub_stray_input(inp)
    print(f"[prepare_input] SEED_D={seed_d} input/ ready (seed + LUT only)")


if __name__ == "__main__":
    main()
