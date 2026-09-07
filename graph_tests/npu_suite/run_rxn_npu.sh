#!/bin/bash
# B3 下一刀：多轮粘性（默认仅 C2 × 7，对齐历史第7轮挂）
#
# Usage:
#   bash run_rxn_npu.sh -v Ascend910B4
#   TOY_ROUNDS=7 bash run_rxn_npu.sh
#   RXN_CASE=C1 TOY_ROUNDS=7 bash run_rxn_npu.sh   # 可选改档
#
# 只打字回报 REPORT: 行；勿回传文件。
set -uo pipefail

SUITE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOC_VERSION="Ascend910B4"
RXN_CASE="${RXN_CASE:-C2}"
TOY_ROUNDS="${TOY_ROUNDS:-7}"

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

export TOY_ROUNDS
export NPU_SKIP_GOLDEN=1
export TOY_SKIP_GOLDEN=1
export TOY_NPU_HOST_ONLY=1

case "${RXN_CASE}" in
C0) CASE_DIR="${SUITE_DIR}/cases/C0-e01-handshake"; export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-600}" ;;
C1) CASE_DIR="${SUITE_DIR}/cases/C1-e13-glue"; export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-3600}" ;;
C2) CASE_DIR="${SUITE_DIR}/cases/C2-e15-a2x2"; export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-5400}" ;;
*)
    echo "[ERROR] RXN_CASE must be C0/C1/C2, got ${RXN_CASE}" >&2
    exit 1
    ;;
esac

echo "============================================================"
echo "===== NPU_SUITE R×N  CASE=${RXN_CASE}  rounds=${TOY_ROUNDS} ====="
echo "===== 历史对齐：单轮绿后查第 N 轮是否粘性挂 ====="
echo "===== 盯 REPORT: / TIMEOUT124 ====="
echo "============================================================"

set +e
bash "${CASE_DIR}/run.sh" -r npu -v "${SOC_VERSION}"
rc=$?
set -e

echo ""
echo "============================================================"
if [ "${rc}" -eq 0 ]; then
    echo "SUMMARY R×N ${RXN_CASE}x${TOY_ROUNDS}:OK"
elif [ "${rc}" -eq 124 ]; then
    echo "SUMMARY R×N ${RXN_CASE}x${TOY_ROUNDS}:TIMEOUT124"
else
    echo "SUMMARY R×N ${RXN_CASE}x${TOY_ROUNDS}:FAIL${rc}"
fi
echo "===== 请把 REPORT:/SUMMARY 打字发回 ====="
echo "============================================================"
exit "${rc}"
