#!/bin/bash
# poly8 block s12-matmul：Stage1 Split (blockDim=1) + Stage2 Matmul (launch_profile)
#   LAUNCH_PROFILE=aicore=2  # 2 launch block，见 int8-matmul-cube-16x256x512
CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT="$(cd "${CURRENT_DIR}/../../.." && pwd)"
SIM_DIRECT="${SIM_DIRECT:-1}"
BUILD_TYPE="Debug"
INSTALL_PREFIX="${CURRENT_DIR}/out"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"
export LAUNCH_PROFILE="${LAUNCH_PROFILE:-aicore=1}"
export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-15}"

while [[ $# -gt 0 ]]; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    *) shift ;;
    esac
done

# SIM CAModel 较慢；未显式设置时默认 120s（与 int8-matmul SIM 实测 ~10s 留足余量）
if [ "${RUN_MODE}" = "sim" ] && [ -z "${KERNEL_COMPUTE_BUDGET_SEC_SET:-}" ]; then
    case "${KERNEL_COMPUTE_BUDGET_SEC}" in
    15) export KERNEL_COMPUTE_BUDGET_SEC=120 ;;
    esac
fi

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
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH}"
fi

set -e
# golden a0/a1 from block-s123
BLOCK_S123="${CURRENT_DIR}/../frozen-fix-merged-kyber-ntt256-limb6-poly8-block-s123"
if [ ! -f "${BLOCK_S123}/output/a0.bin" ]; then
    echo "[run.sh] generating block-s123 stage2 ref (a0/a1)..."
    (cd "${BLOCK_S123}" && KERNEL_COMPUTE_BUDGET_SEC=15 bash run.sh -r cpu -v "${SOC_VERSION}" >/dev/null)
fi

_run_once() {
    local kernel_mode="$1"
    rm -rf build out
    cmake -B build -DRUN_MODE="${RUN_MODE}" -DSOC_VERSION="${SOC_VERSION}" \
        -DS12_KERNEL_MODE="${kernel_mode}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}"
    cmake --build build -j
    cmake --install build
    rm -f ascendc_kernels_bbit
    cp ./out/bin/ascendc_kernels_bbit ./
    export LD_LIBRARY_PATH="$(pwd)/out/lib:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH}"
    if [ "${RUN_MODE}" = "sim" ]; then
        source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
    fi
    bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
    if [ "${RUN_MODE}" = "sim" ]; then
        camodel_sim_collect_stray "${CURRENT_DIR}"
    fi
}

rm -rf input output
mkdir -p input output
python3 "${CURRENT_DIR}/scripts/gen_data.py"

if [ "${RUN_MODE}" = "sim" ]; then
    # SIM：双 kernel 混编 Matmul 会挂；拆 mmad(stage1) + stage2 两次单 kernel 构建
    export SIM_STAGE1_ONLY=1
    _run_once mmad
    export SIM_STAGE2_ONLY=1
    unset SIM_STAGE1_ONLY
    _run_once stage2
    unset SIM_STAGE2_ONLY
else
    _run_once both
fi

python3 "${CURRENT_DIR}/scripts/verify_result.py"
echo "[SUCCESS] fix-merged-kyber-ntt256-limb6-poly8-block-s12-matmul (${RUN_MODE} ${LAUNCH_PROFILE})"
