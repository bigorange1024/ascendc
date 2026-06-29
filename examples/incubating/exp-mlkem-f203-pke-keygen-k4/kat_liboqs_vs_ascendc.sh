#!/usr/bin/env bash
# liboqs PKE KeyGen ↔ AscendC：默认 10×CPU + 1×SIM，每轮随机 SEED_D、两边同种子。
# 用法：bash kat_liboqs_vs_ascendc.sh
# 可选：KAT_CPU_COUNT=10 KAT_SIM_COUNT=1 bash kat_liboqs_vs_ascendc.sh
# 调试：KAT_VERBOSE=1 bash kat_liboqs_vs_ascendc.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "${ROOT}/scripts/kat_liboqs_vs_ascendc.py" "$@"
