#!/bin/bash
# RB-T07-prep-shell：Encrypt prep Host 壳（ρ←ek 尾；coins→y/e1/e2）
#
# Usage（默认 = Host CPU 对拍；本刀无设备核，不跑 SIM/NPU）：
#   bash run.sh -r cpu -v Ascend910B4
#
# 说明：TASK 验收「CPU；设备可选」——本刀仅 Host 编排 + shared CBD 积木，
#       -r sim / -r npu 直接跳过并标 skip（非失败）。
#
# 与 KeyGen SEED 差异：PRF 种子直接是 coins，不用 SEED_D→G→σ。
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)
_ORIG_ARGS=("$@")
# graph-tests/enc_related/<case> → 仓库根 ../../..
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

echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} RB-T07=host-prep-only"

if [ "${RUN_MODE}" = "sim" ] || [ "${RUN_MODE}" = "npu" ]; then
    echo "[SKIP] RB-T07 无设备核；RUN_MODE=${RUN_MODE} 非本刀验收范围（Host CPU 已覆盖）"
    exit 0
fi

set -e
rm -rf "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"
mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"

# Host 编排：固定 ek/coins → ρ + (y,e1,e2) + golden
python3 "${CURRENT_DIR}/scripts/prep_host.py"
python3 "${CURRENT_DIR}/scripts/verify_result.py"
echo "[SUCCESS] RB-T07-prep-shell (cpu/host)"
