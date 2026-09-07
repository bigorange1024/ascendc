#!/bin/bash
# C2 — Â 2×2 SampleNTT + 粘合（包装 toy-e15）
#
# NPU（1 轮；跳过 golden）：
#   bash run.sh -r npu -v Ascend910B4
#
# SIM smoke：
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${CASE_DIR}/../../../.." && pwd)"
ORIGIN="${REPO_ROOT}/graph_tests/toys/toy-e15-samplentt-a-full-2x2"

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

if [ "${NPU_SKIP_GOLDEN:-1}" = "1" ]; then
    export TOY_SKIP_GOLDEN=1
fi

if [ "${RUN_MODE}" = "npu" ]; then
    export TOY_NPU_HOST_ONLY="${TOY_NPU_HOST_ONLY:-1}"
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-900}"
elif [ "${RUN_MODE}" = "sim" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-1800}"
fi

echo "[C2] origin=${ORIGIN} RUN_MODE=${RUN_MODE} TOY_ROUNDS=${TOY_ROUNDS} TOY_SKIP_GOLDEN=${TOY_SKIP_GOLDEN:-0} TOY_NPU_HOST_ONLY=${TOY_NPU_HOST_ONLY:-0} budget=${KERNEL_COMPUTE_BUDGET_SEC}"

cd "${ORIGIN}"
exec bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}" "${EXTRA[@]}"
