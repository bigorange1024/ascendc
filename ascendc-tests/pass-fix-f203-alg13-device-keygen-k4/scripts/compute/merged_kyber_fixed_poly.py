# @probe pass-fix-f203-alg13-device-keygen-k4
# @file scripts/compute/merged_kyber_fixed_poly.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `merged_kyber_fixed_poly.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps. compute 子树可单独跑中间 bin（调试）。 / Compute subtree debug bins optional.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。

"""固定 Kyber poly（seed=42），供 vec-k4-v2 探针 golden 与 NTT 对拍。"""
from __future__ import annotations

import numpy as np

Q = 3329
_SEED = 42
_rng = np.random.default_rng(_SEED)
FIXED_POLY = _rng.integers(0, Q, size=256, dtype=np.int32)
FIXED_E_POLY = np.random.default_rng(_SEED + 1).integers(0, Q, size=256, dtype=np.int32)
