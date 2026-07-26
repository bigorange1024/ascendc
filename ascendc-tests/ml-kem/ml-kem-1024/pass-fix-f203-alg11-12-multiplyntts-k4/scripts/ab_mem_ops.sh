#!/usr/bin/env bash
# A/B: ALG11_MEM_OPS=1 (GM ROM DataCopy) vs 0 (SetValue/CreateVecIndex legacy)
set -euo pipefail
CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOC="${SOC_VERSION:-Ascend910B4}"
RUN_MODE="${1:-sim}"

run_one() {
    local mem_ops="$1"
    echo "========== ALG11_MEM_OPS=${mem_ops} RUN_MODE=${RUN_MODE} =========="
    ALG11_MEM_OPS="${mem_ops}" bash "${CASE_DIR}/run.sh" -r "${RUN_MODE}" -v "${SOC}" 2>&1 \
        | tee "/tmp/alg11_mem_ops_${mem_ops}_${RUN_MODE}.log" \
        | rg "Model RUN TIME|Total tick|\[OK\] verify|\[SUCCESS\]|error:" || true
}

run_one 1
run_one 0

echo ""
echo "========== tick summary =========="
for m in 1 0; do
    log="/tmp/alg11_mem_ops_${m}_${RUN_MODE}.log"
    if [[ -f "${log}" ]]; then
        echo "--- MEM_OPS=${m} ---"
        rg "Model RUN TIME|Total tick|\[OK\] verify" "${log}" || true
    fi
done
