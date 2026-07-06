#!/bin/bash
# fix-f203-alg14-lines3-15-encrypt-prep-k4 — Alg.14 prep：Â + r/e₁/e₂（单 launch，stable vendored）
#
# Usage（默认）：
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#
# 环境变量：
#   COINS_SEED — 默认 20260706
#   ENCRYPT_PREP_VENDOR_SYNC — 1 时从 stable 刷新 prep/（默认 0，prep 已 vendored）
#   KERNEL_COMPUTE_BUDGET_SEC — 默认 600

CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)
REPO_ROOT="$(cd "${CURRENT_DIR}/../.." && pwd)"

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
export COINS_SEED="${COINS_SEED:-20260706}"
export F203_ALG7_REJ_IMPL="${F203_ALG7_REJ_IMPL:-1}"
export F203_ALG7_D12_GATHER="${F203_ALG7_D12_GATHER:-0}"
export F203_AHAT16_BLOCK_DIM="${F203_AHAT16_BLOCK_DIM:-2}"
export F203_ALG7_XOF_504="${F203_ALG7_XOF_504:-0}"
export F203_CBD_BLOCK_DIM="${F203_CBD_BLOCK_DIM:-1}"

echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} COINS_SEED=${COINS_SEED} blockDim=${F203_AHAT16_BLOCK_DIM}"

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
if [ "${ENCRYPT_PREP_VENDOR_SYNC:-0}" = "1" ] || [ ! -d "${CURRENT_DIR}/prep/ahat" ]; then
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_stable_keygen.sh"
fi

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
    -DF203_ALG7_XOF_504=${F203_ALG7_XOF_504} \
    -DF203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM}
cmake --build build -j
cmake --install build

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf output
mkdir -p input output

export F203_ALG7_XOF_504="${F203_ALG7_XOF_504:-0}"
python3 "${CURRENT_DIR}/scripts/prep/gen_alg7_interleave_rom.py"
python3 "${CURRENT_DIR}/scripts/prep/gen_alg7_deinterleave_rom.py"
python3 "${CURRENT_DIR}/scripts/prep/gen_alg7_compact_lut.py"

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
echo "[SUCCESS] fix-f203-alg14-lines3-15-encrypt-prep-k4 (${RUN_MODE})"
