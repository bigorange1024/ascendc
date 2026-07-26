#!/usr/bin/env python3
# coding=utf-8
"""prepare_production_input.py — Alg.19 KEM KeyGen 生产 input（seed_d + LUT）。

流水线位置：run.sh 在 kernel 前调用；写出 input/seed_d.bin 与 lut_*。
seed_d：默认 SHA3 派生（resolve_host_seed_d）；SEED_D env 可覆盖定点。
与 golden：仅准备设备输入，不写 golden（golden 由 gen_data.py / liboqs）。
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
_STRAY_INPUT_PRODUCTION_ONLY = ("kem_seed.bin",)


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
    from resolve_host_seed_d import resolve_host_seed_d

    seed_d, how = resolve_host_seed_d()
    inp = ROOT / "input"
    inp.mkdir(exist_ok=True)
    (inp / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
    write_lut_if_missing(inp)
    scrub_stray_input(inp)
    print(f"[prepare_input] SEED_D={seed_d} via={how} input/ ready (seed + LUT only)")


if __name__ == "__main__":
    main()
