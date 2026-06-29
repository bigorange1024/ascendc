#!/usr/bin/env python3
# @probe pass-fix-f203-alg13-device-keygen-k4
# @file scripts/host_ek_append.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `host_ek_append.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。

# coding=utf-8
"""
Alg.13 行 21 Host 拼接：ek_PKE = ByteEncode₁₂(t̂) polyvec ‖ ρ。

用于 G4 全链减少一次设备 Launch（Launch E 仅 G1 门禁保留设备核验收）。
输入/输出均在 keygen 探针 output/ 目录。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "output"

EK_POLYVEC_BYTES = 4 * 384  # 1536
RHO_BYTES = 32
EK_PKE_BYTES = EK_POLYVEC_BYTES + RHO_BYTES


def host_ek_append(out_dir: Path = OUT) -> None:
    ek_path = out_dir / "ek_polyvec.bin"
    rho_path = out_dir / "rho.bin"
    if not ek_path.is_file():
        raise SystemExit(f"missing {ek_path} (vec-k4-v2 行 19)")
    if not rho_path.is_file():
        raise SystemExit(f"missing {rho_path} (G3/Â 段 ρ)")

    ek = np.fromfile(ek_path, dtype=np.uint8)
    rho = np.fromfile(rho_path, dtype=np.uint8)
    if ek.size != EK_POLYVEC_BYTES:
        raise SystemExit(f"ek_polyvec bytes {ek.size} != {EK_POLYVEC_BYTES}")
    if rho.size != RHO_BYTES:
        raise SystemExit(f"rho bytes {rho.size} != {RHO_BYTES}")

    ek_pke = np.concatenate([ek, rho])
    if ek_pke.size != EK_PKE_BYTES:
        raise SystemExit(f"ek_pke bytes {ek_pke.size} != {EK_PKE_BYTES}")

    ek_pke.tofile(out_dir / "ek_pke.bin")
    print(f"[keygen] host ek_append: {EK_POLYVEC_BYTES}B + {RHO_BYTES}B -> ek_pke.bin ({EK_PKE_BYTES}B)")


def main() -> int:
    host_ek_append()
    return 0


if __name__ == "__main__":
    sys.exit(main())
