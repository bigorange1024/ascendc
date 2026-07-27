#!/bin/bash
# @probe pass-fix-f203-alg13-device-keygen-k2
# @file scripts/dev/sim_prep.sh
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `sim_prep.sh`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。

# 分段 SIM — 仅 prep（行 3–15）。全链验收前单独调试用。
# 前置：bash run.sh -r cpu 已生成 golden（或至少 scripts/gen_data.py）。
# Usage: bash scripts/dev/sim_prep.sh
set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
exec env KEYGEN_SIM_STAGE=prep bash "${ROOT}/run.sh" -r sim -v Ascend910B4
