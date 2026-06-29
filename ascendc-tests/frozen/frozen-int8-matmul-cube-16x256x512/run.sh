#!/bin/bash
# Kyber Stage2 原生 int8 Matmul：C[16,512]=A[16,256]xB[256,512]（无垫片）
# Usage:
#   bash run.sh -r cpu -v Ascend910B4                    # 默认 aicore=1
#   LAUNCH_PROFILE=aicore=2 bash run.sh -r cpu -v ...    # 2 launch block
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
export LAUNCH_PROFILE="${LAUNCH_PROFILE:-aicore=1}"
CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT="$(cd "${CURRENT_DIR}/../../.." && pwd)"
SIM_DIRECT="${SIM_DIRECT:-1}"
BUILD_TYPE="Debug"
INSTALL_PREFIX="${CURRENT_DIR}/out"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"
export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-15}"

while [[ $# -gt 0 ]]; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    *) shift ;;
    esac
done

if [ -d "$HOME/Ascend/ascend-toolkit/latest" ]; then
    _ASCEND_INSTALL_PATH="$HOME/Ascend/ascend-toolkit/latest"
elif [ -d "$HOME/Ascend/cann" ]; then
    _ASCEND_INSTALL_PATH="$HOME/Ascend/cann"
else
    _ASCEND_INSTALL_PATH=/usr/local/Ascend/ascend-toolkit/latest
fi
export ASCEND_TOOLKIT_HOME="${_ASCEND_INSTALL_PATH}"
export ASCEND_HOME_PATH="${_ASCEND_INSTALL_PATH}"

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
elif [ -f "${_ASCEND_INSTALL_PATH}/bin/setenv.bash" ]; then
    source "${_ASCEND_INSTALL_PATH}/bin/setenv.bash"
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH}"
fi

BLOCK_S123="${CURRENT_DIR}/../frozen-fix-merged-kyber-ntt256-limb6-poly8-block-s123"
if [ ! -f "${BLOCK_S123}/output/s0.bin" ]; then
    echo "[run.sh] generating block-s123 s0 ref..."
    (cd "${BLOCK_S123}" && KERNEL_COMPUTE_BUDGET_SEC=15 bash run.sh -r cpu -v "${SOC_VERSION}" >/dev/null)
fi

set -e
rm -rf build out
cmake -B build -DRUN_MODE="${RUN_MODE}" -DSOC_VERSION="${SOC_VERSION}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}"
cmake --build build -j
cmake --install build

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output
mkdir -p input output
python3 "${CURRENT_DIR}/scripts/gen_data.py"
export LD_LIBRARY_PATH="$(pwd)/out/lib:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH}"

if [ "${RUN_MODE}" = "sim" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi
bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

python3 "${CURRENT_DIR}/scripts/verify_result.py"
echo "[SUCCESS] int8-matmul-cube-16x256x512 (${RUN_MODE} ${LAUNCH_PROFILE})"
