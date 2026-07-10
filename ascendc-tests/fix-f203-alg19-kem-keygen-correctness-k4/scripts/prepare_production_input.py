#!/usr/bin/env python3
# coding=utf-8
"""
prepare_production_input.py — Alg.19 KeyGen 生产 input 准备（本探针自有入口）。

作用：
  - 写 input/seed_d.bin（uint32 LE，默认 SEED_D=20260619）
  - LUT 缺失时调用 scripts/compute/gen_data 生成 lut_even/odd_stacked.bin
  - 清掉 a_hat/src/rho 等中间态；生产路径（KEM_KG_EXT_SEED≠1）再清 kem_seed.bin

契约：input/ 仅 seed + 静态 NTT LUT；禁止预填 PKE 中间 GM。
注意：scripts/compute 为 PKE 辅助脚本树，本文件只做胶水，不改其算法。
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
# kat / KEM_KG_EXT_SEED=1 旁路 A 专用；生产路径（extseed=0）须清掉，避免 input/ 混态。
_STRAY_INPUT_PRODUCTION_ONLY = ("kem_seed.bin",)


def write_lut_if_missing(inp: Path) -> None:
    """若 even/odd stacked LUT 不齐，从 compute_gen 加载并写盘。"""
    lut_even = inp / "lut_even_stacked.bin"
    lut_odd = inp / "lut_odd_stacked.bin"
    if lut_even.is_file() and lut_odd.is_file():
        return
    lut = compute_gen.load_lut_t_i8()
    compute_gen.lut_planar_stacked(lut, even=True).tofile(lut_even)
    compute_gen.lut_planar_stacked(lut, even=False).tofile(lut_odd)
    print(f"[prepare_input] wrote LUT -> {lut_even.name}, {lut_odd.name}")


def scrub_stray_input(inp: Path) -> None:
    """删除不应出现在生产 input/ 的中间态文件。"""
    extseed = os.environ.get("KEM_KG_EXT_SEED", "0") == "1"
    names = list(_STRAY_INPUT)
    if not extseed:
        names.extend(_STRAY_INPUT_PRODUCTION_ONLY)
    for name in names:
        p = inp / name
        if p.is_file():
            p.unlink()
            print(f"[prepare_input] removed stray input/{name}")


def main() -> None:
    """准备 seed_d + LUT，并 scrub 杂散 input。"""
    seed_d = int(os.environ.get("SEED_D", "20260619"))
    inp = ROOT / "input"
    inp.mkdir(exist_ok=True)
    (inp / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
    write_lut_if_missing(inp)
    scrub_stray_input(inp)
    print(f"[prepare_input] SEED_D={seed_d} input/ ready (seed + LUT only)")


if __name__ == "__main__":
    main()
