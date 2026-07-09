#!/usr/bin/env bash
# liboqs PKE Encrypt ↔ exp-mlkem-f203-pke-encrypt-k4 对拍
# 默认 CPU×10 + SIM×1（每轮随机 SEED_D → liboqs fixture → AscendC c）
#
#   bash kat_liboqs_vs_ascendc.sh
#   KAT_CPU_COUNT=10 KAT_SIM_COUNT=1 bash kat_liboqs_vs_ascendc.sh
#   KAT_VERBOSE=1 bash kat_liboqs_vs_ascendc.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "${ROOT}/scripts/kat_liboqs_vs_ascendc.py" "$@"
