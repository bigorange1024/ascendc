#!/usr/bin/env python3
# coding=utf-8
"""Legacy：mixPass=5→4 分段调试时 pass5 后 Host 重算 mat_c/dst（G4 / mixPass=0 生产路径已废弃，勿用）。"""
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("gen_data", ROOT / "scripts" / "gen_data.py")
gen = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(gen)

import numpy as np


def main() -> int:
    out = ROOT / "output"
    s0_path = out / "_checkpoint_s0.bin"
    if not s0_path.is_file():
        s0_path = out / "s0.bin"
    if not s0_path.is_file():
        raise SystemExit("[cpu_fixup] missing s0.bin / _checkpoint_s0.bin")

    s0 = np.fromfile(s0_path, dtype=np.int8).reshape(gen.M_ROWS, gen.N)
    lut = gen.load_lut_t_i8()
    c_lo_e, c_lo_o, c_hi_e, c_hi_o = gen.mat_c_tmp_golden(s0, lut)
    mat_c = gen.pack_mat_c_planar(c_lo_e, c_lo_o, c_hi_e, c_hi_o)
    dst = gen.golden_dst_from_planar(mat_c)

    mat_c.astype(np.int32).tofile(out / "mat_c.bin")
    mat_c.astype(np.int32).tofile(out / "_checkpoint_mat_c.bin")
    dst.astype(np.int32).tofile(out / "dst.bin")

    inp = ROOT / "input"
    inp.mkdir(exist_ok=True)
    dst.astype(np.int32).tofile(inp / "dst_preset.bin")
    print("[pass5_fixup] recomputed mat_c + dst from device s0 (S2 MMAD host workaround)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
