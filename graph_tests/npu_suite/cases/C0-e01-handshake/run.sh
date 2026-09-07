#!/bin/bash
# C0 — 2-launch + SET(4) 握手壳（包装 toy-e01）
#
# NPU（套件默认 1 轮）：
#   bash run.sh -r npu -v Ascend910B4
#
# SIM smoke：
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#
# 调试：OMIT_SET4 请直接进 origin toy（本包装不传故障注入）
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${CASE_DIR}/../../../.." && pwd)"
ORIGIN="${REPO_ROOT}/graph_tests/toys/toy-e01-2launch-set4-trace-repeat"

if [ ! -x "${ORIGIN}/run.sh" ]; then
    echo "[ERROR] missing origin toy: ${ORIGIN}/run.sh" >&2
    exit 1
fi

RUN_MODE=""
SOC_VERSION=""
EXTRA=()
while [ $# -gt 0 ]; do
    case "$1" in
    -r | --run-mode)
        RUN_MODE="$2"
        shift 2
        ;;
    -v | --soc-version)
        SOC_VERSION="$2"
        shift 2
        ;;
    *)
        EXTRA+=("$1")
        shift
        ;;
    esac
done

: "${RUN_MODE:=npu}"
: "${SOC_VERSION:=Ascend910B4}"

export TOY_ROUNDS="${TOY_ROUNDS:-1}"
export OMIT_SET4="${OMIT_SET4:-0}"

if [ "${RUN_MODE}" = "npu" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-120}"
elif [ "${RUN_MODE}" = "sim" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-600}"
fi

echo "[C0] origin=${ORIGIN} RUN_MODE=${RUN_MODE} TOY_ROUNDS=${TOY_ROUNDS} budget=${KERNEL_COMPUTE_BUDGET_SEC}"

cd "${ORIGIN}"
exec bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}" "${EXTRA[@]}"
