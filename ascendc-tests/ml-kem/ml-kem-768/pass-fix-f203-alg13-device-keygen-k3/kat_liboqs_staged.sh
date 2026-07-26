#!/usr/bin/env bash
# @probe pass-fix-f203-alg13-device-keygen-k3
# @file kat_liboqs_staged.sh
# @layer legacy
# @role 分阶段 KAT（legacy staging I/O）。 / Staged KAT (legacy I/O).
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1184B) + dk_pke.bin (1152B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps. 本文件可能用于 legacy/staged I/O 或分阶段调试，非默认生产路径。 / May use legacy staging I/O.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends 见同目录 INDEX/STATUS 或相邻 entry/host 调用链。
# @verify KEYGEN_KAT=1 bash run.sh 或 kat_*.sh；对比 liboqs。

set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "${ROOT}/scripts/kat_liboqs_staged.py" "$@"
