#!/bin/bash
# 实机整份串行：C0 → C1 → C2（禁止并行）
#
# Usage:
#   bash run_all_npu.sh -v Ascend910B4
#   TOY_ROUNDS=1 bash run_all_npu.sh   # 默认已是 1
set -euo pipefail

SUITE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOC_VERSION="Ascend910B4"

while [ $# -gt 0 ]; do
    case "$1" in
    -v | --soc-version)
        SOC_VERSION="$2"
        shift 2
        ;;
    *)
        echo "[ERROR] unknown arg: $1" >&2
        exit 1
        ;;
    esac
done

export TOY_ROUNDS="${TOY_ROUNDS:-1}"
export NPU_SKIP_GOLDEN="${NPU_SKIP_GOLDEN:-1}"

echo "=== NPU_SUITE start SOC=${SOC_VERSION} TOY_ROUNDS=${TOY_ROUNDS} ==="

_run_case() {
    local name="$1"
    local dir="$2"
    echo ""
    echo ">>> ${name} (${dir})"
    bash "${dir}/run.sh" -r npu -v "${SOC_VERSION}"
    echo "<<< ${name} OK"
}

_run_case "C0-e01-handshake" "${SUITE_DIR}/cases/C0-e01-handshake"
_run_case "C1-e13-glue" "${SUITE_DIR}/cases/C1-e13-glue"
_run_case "C2-e15-a2x2" "${SUITE_DIR}/cases/C2-e15-a2x2"

echo ""
echo "[SUCCESS] NPU_SUITE all cases finished (C0→C1→C2)"
