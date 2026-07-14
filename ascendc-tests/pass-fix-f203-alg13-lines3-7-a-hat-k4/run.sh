#!/bin/bash
# pass-fix-f203-alg13-lines3-7-a-hat-k4 — Alg.13 行 3–7：16× SampleNTT 向量（逐 poly SHAKE + UB 全链）
#
# Usage（默认）：
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#
# 环境变量：
#   SEED_D — derand 种子（默认 20260619）
#   F203_ALG7_XOF_504 — 0=672B（默认）；1=504B 对照实验
#   KERNEL_COMPUTE_BUDGET_SEC — 默认 600（16 poly SIM 墙钟 ~333s）
#
#
# 多环境分流（scripts/runtime_env.sh）：
#   bash run.sh -r auto -v Ascend910B4      # 单档最优 npu>sim>cpu（≠完整验收）
#   bash run.sh -r verify -v Ascend910B4    # cpu → SIM_DIRECT sim [→ npu，非WSL]
#   WSL 禁止 -r npu；说明见 docs/engineering/NPU真机环境说明.md
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)
_ORIG_ARGS=("$@")
REPO_ROOT="$(cd "${CURRENT_DIR}/../.." && pwd)"
ALG7_DIR="${CURRENT_DIR}/../pass-fix-f203-alg7-sample-ntt-k4"

BUILD_TYPE="Debug"
INSTALL_PREFIX="${CURRENT_DIR}/out"
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
    -i | --install-path) ASCEND_INSTALL_PATH="$2"; shift 2 ;;
    -b | --build-type) BUILD_TYPE="$2"; shift 2 ;;
    -p | --install-prefix) INSTALL_PREFIX="$2"; shift 2 ;;
    --) shift; break ;;
    *) echo "[ERROR] Unexpected option: $1"; exit 1 ;;
    esac
done


# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/runtime_env.sh"
export ASCENDC_CASE_SUPPORTS_NPU="${ASCENDC_CASE_SUPPORTS_NPU:-1}"
runtime_env_dispatch "${BASH_SOURCE[0]}" "${_ORIG_ARGS[@]}"
if [ -n "$ASCEND_INSTALL_PATH" ]; then
    _ASCEND_INSTALL_PATH=$ASCEND_INSTALL_PATH
elif [ -n "$ASCEND_HOME_PATH" ]; then
    _ASCEND_INSTALL_PATH=$ASCEND_HOME_PATH
elif [ -d "$HOME/Ascend/ascend-toolkit/latest" ]; then
    _ASCEND_INSTALL_PATH=$HOME/Ascend/ascend-toolkit/latest
elif [ -d "$HOME/Ascend/cann" ]; then
    _ASCEND_INSTALL_PATH=$HOME/Ascend/cann
else
    _ASCEND_INSTALL_PATH=/usr/local/Ascend/ascend-toolkit/latest
fi

export ASCEND_TOOLKIT_HOME=${_ASCEND_INSTALL_PATH}
export ASCEND_HOME_PATH=${_ASCEND_INSTALL_PATH}
export SEED_D="${SEED_D:-20260619}"
export F203_ALG7_REJ_IMPL="${F203_ALG7_REJ_IMPL:-1}"
export F203_ALG7_D12_GATHER="${F203_ALG7_D12_GATHER:-0}"
export F203_AHAT16_BLOCK_DIM="${F203_AHAT16_BLOCK_DIM:-2}"
export F203_ALG7_XOF_504="${F203_ALG7_XOF_504:-0}"

echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} SEED_D=${SEED_D} F203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL} blockDim=${F203_AHAT16_BLOCK_DIM} F203_ALG7_XOF_504=${F203_ALG7_XOF_504}"

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
elif [ -f "${_ASCEND_INSTALL_PATH}/bin/setenv.bash" ]; then
    source "${_ASCEND_INSTALL_PATH}/bin/setenv.bash"
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
fi

set -e
rm -rf build out
mkdir -p build
cmake -B build \
    -DRUN_MODE=${RUN_MODE} \
    -DSOC_VERSION=${SOC_VERSION} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
    -DASCEND_CANN_PACKAGE_PATH=${_ASCEND_INSTALL_PATH} \
    -DF203_ALG7_REJ_IMPL=${F203_ALG7_REJ_IMPL} \
    -DF203_ALG7_D12_GATHER=${F203_ALG7_D12_GATHER} \
    -DF203_AHAT16_BLOCK_DIM=${F203_AHAT16_BLOCK_DIM} \
    -DF203_ALG7_XOF_504=${F203_ALG7_XOF_504}
cmake --build build -j
cmake --install build

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output
mkdir -p input output

# ROM 与 alg7 几何一致（504B 实验须 export F203_ALG7_XOF_504=1）
export F203_ALG7_XOF_504="${F203_ALG7_XOF_504:-0}"
python3 "${ALG7_DIR}/scripts/gen_alg7_interleave_rom.py"
python3 "${ALG7_DIR}/scripts/gen_alg7_deinterleave_rom.py"
python3 "${ALG7_DIR}/scripts/gen_alg7_compact_lut.py"

python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH

export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-600}"
if [ "${RUN_MODE}" = "sim" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi
bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

python3 "${CURRENT_DIR}/scripts/verify_result.py"
echo "[SUCCESS] pass-fix-f203-alg13-lines3-7-a-hat-k4 (${RUN_MODE})"
