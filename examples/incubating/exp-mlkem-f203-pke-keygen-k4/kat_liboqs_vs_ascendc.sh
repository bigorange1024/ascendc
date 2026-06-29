#!/usr/bin/env bash
# @probe exp-mlkem-f203-pke-keygen-k4
# @file kat_liboqs_vs_ascendc.sh
# @layer legacy
# @role KAT 外壳脚本：调用 liboqs 与 run.sh。 / KAT wrapper shell.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends 见同目录 INDEX/STATUS 或相邻 entry/host 调用链。
# @verify KEYGEN_KAT=1 bash run.sh 或 kat_*.sh；对比 liboqs。

# liboqs PKE KeyGen ↔ 探针 G4 对拍（默认 CPU×10 + SIM×1，每轮随机 SEED_D）
#   bash kat_liboqs_vs_ascendc.sh
#   KAT_CPU_COUNT=10 KAT_SIM_COUNT=1 bash kat_liboqs_vs_ascendc.sh
#   KAT_VERBOSE=1 bash kat_liboqs_vs_ascendc.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "${ROOT}/scripts/kat_liboqs_vs_ascendc.py" "$@"
