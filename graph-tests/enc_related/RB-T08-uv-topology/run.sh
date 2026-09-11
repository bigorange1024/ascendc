#!/bin/bash
# RB-T08-uv-topology：Encrypt 域拼装 G3（预喂 Â/ŷ/t̂/e₁/e₂/μ → u,v）
#
# Usage（默认 = Host CPU 对拍；本刀无设备 MIX 核）：
#   bash run.sh -r cpu -v Ascend910B4
#
# 调试 / 非默认：
#   bash run.sh -r sim -v Ascend910B4   # 显式 SKIP（无 AscendC）
#
# 拓扑：u=INTT(Âᵀ∘ŷ)+e₁ ； v=INTT(⟨t̂,ŷ⟩)+e₂+μ
# 禁抄 alg14/encrypt；未用 CrossCore（无 flag 5/7 问题）。
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)
REPO_ROOT="$(cd "${CURRENT_DIR}/../../.." && pwd)"

SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"

SHORT=r:,v:,i:,b:,p:
LONG=run-mode:,soc-version:,install-path:,build-type:,install-prefix:
OPTS=$(getopt -a --options $SHORT --longoptions $LONG -- "$@")
eval set -- "$OPTS"

while :; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    -i | --install-path) shift 2 ;;
    -b | --build-type) shift 2 ;;
    -p | --install-prefix) shift 2 ;;
    --) shift; break ;;
    *) echo "[ERROR] Unexpected option: $1"; exit 1 ;;
    esac
done

echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} RB-T08=host-uv-topology"

if [ "${RUN_MODE}" = "sim" ] || [ "${RUN_MODE}" = "npu" ]; then
    echo "[SKIP] RB-T08 无 AscendC MIX 核；RUN_MODE=${RUN_MODE} 非本刀验收范围（Host CPU 已覆盖拓扑）"
    exit 0
fi

set -e
rm -rf "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"
mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"

# 1) 预生成中间量 + golden
python3 "${CURRENT_DIR}/scripts/gen_data.py"
# 2) Host 孪生拼装 u,v
python3 "${CURRENT_DIR}/scripts/uv_topology_host.py"
# 3) 对拍
python3 "${CURRENT_DIR}/scripts/verify_result.py"
echo "[SUCCESS] RB-T08-uv-topology (cpu/host)"
