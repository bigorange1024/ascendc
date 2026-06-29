#!/bin/bash
# DEPRECATED (2026-06-10): 勿再使用。见 STATUS.md；替代：ascendc-tests/frozen/frozen-fix-merged-kyber-ntt256-limb6-poly8-s123
# exp-mlkem-f203-stage12-encode-matmul-mix: Stage1+2 MIX 融合（cpu / sim / npu）
# Usage: bash run.sh -r cpu -v Ascend910B4 --aicore 1
echo "WARNING: this example is DEPRECATED. See STATUS.md." >&2
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)

BUILD_TYPE="Debug"
INSTALL_PREFIX="${CURRENT_DIR}/out"
LAUNCH_PROFILE="${LAUNCH_PROFILE:-aicore=1}"

SHORT=r:,v:,i:,b:,p:,P:
LONG=run-mode:,soc-version:,install-path:,build-type:,install-prefix:,profile:,aicore:
OPTS=$(getopt -a --options $SHORT --longoptions $LONG -- "$@")
eval set -- "$OPTS"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"

while :; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    -i | --install-path) ASCEND_INSTALL_PATH="$2"; shift 2 ;;
    -b | --build-type) BUILD_TYPE="$2"; shift 2 ;;
    -p | --install-prefix) INSTALL_PREFIX="$2"; shift 2 ;;
    -P | --profile) LAUNCH_PROFILE="$2"; shift 2 ;;
    --aicore) LAUNCH_PROFILE="aicore=$2"; shift 2 ;;
    --) shift; break ;;
    *) echo "[ERROR] Unexpected option: $1"; exit 1 ;;
    esac
done

if [[ "$LAUNCH_PROFILE" != "aicore=1" ]]; then
    echo "ERROR: LAUNCH_PROFILE must be aicore=1 (首版仅单 AI Core)"
    exit 1
fi

if [ -n "$ASCEND_INSTALL_PATH" ]; then
    _ASCEND_INSTALL_PATH=$ASCEND_INSTALL_PATH
elif [ -n "$ASCEND_HOME_PATH" ]; then
    _ASCEND_INSTALL_PATH=$ASCEND_HOME_PATH
elif [ -d "$HOME/Ascend/ascend-toolkit/latest" ]; then
    _ASCEND_INSTALL_PATH=$HOME/Ascend/ascend-toolkit/latest
elif [ -d "$HOME/Ascend/cann-9.0.0/toolkit" ]; then
    _ASCEND_INSTALL_PATH=$HOME/Ascend/cann-9.0.0/toolkit
elif [ -d "$HOME/Ascend/cann-9.0.0" ]; then
    _ASCEND_INSTALL_PATH=$HOME/Ascend/cann-9.0.0
else
    _ASCEND_INSTALL_PATH=/usr/local/Ascend/ascend-toolkit/latest
fi

export ASCEND_TOOLKIT_HOME=${_ASCEND_INSTALL_PATH}
export ASCEND_HOME_PATH=${_ASCEND_INSTALL_PATH}
export LAUNCH_PROFILE
echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} LAUNCH_PROFILE=${LAUNCH_PROFILE}"

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
elif [ -f "${_ASCEND_INSTALL_PATH}/bin/setenv.bash" ]; then
    source "${_ASCEND_INSTALL_PATH}/bin/setenv.bash"
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
    # shellcheck source=/dev/null
    source "${HOME}/ascendc/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
fi

set -e
rm -rf build out
mkdir -p build
cmake -B build -DRUN_MODE=${RUN_MODE} -DSOC_VERSION=${SOC_VERSION} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
    -DASCEND_CANN_PACKAGE_PATH=${_ASCEND_INSTALL_PATH}
cmake --build build -j
cmake --install build

cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output && mkdir -p input output
python3 scripts/gen_data.py
# 纯计算预算（首次 [TmSim]/Model Start 后至 kernel 正常结束）；不含编译/gen_data/PEM 初始化等。
# wall-clock = 启动余量 + 计算预算。
if [ "${RUN_MODE}" = "sim" ]; then
    KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-120}"
    KERNEL_STARTUP_MARGIN_SEC="${KERNEL_STARTUP_MARGIN_SEC:-30}"
else
    KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-12}"
    KERNEL_STARTUP_MARGIN_SEC="${KERNEL_STARTUP_MARGIN_SEC:-3}"
fi
KERNEL_TIMEOUT_SEC="${KERNEL_TIMEOUT_SEC:-$((KERNEL_STARTUP_MARGIN_SEC + KERNEL_COMPUTE_BUDGET_SEC))}"
export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH
if ! timeout "${KERNEL_TIMEOUT_SEC}" ./ascendc_kernels_bbit; then
    rc=$?
    if [ "$rc" -eq 124 ]; then
        echo "[FAILED] kernel exceeded ${KERNEL_TIMEOUT_SEC}s wall-clock (compute budget=${KERNEL_COMPUTE_BUDGET_SEC}s — hang/deadlock?)"
        exit 124
    fi
    exit "$rc"
fi
echo "=== Compare output vs golden (${LAUNCH_PROFILE}) ==="
md5sum output/mat_c_gm.bin output/golden.bin
if ! cmp -s output/mat_c_gm.bin output/golden.bin; then
    echo "[FAILED] output differs from golden"
    exit 1
fi
python3 scripts/verify_result.py output/mat_c_gm.bin output/golden.bin
echo "[SUCCESS] aicore=1 stage12 fusion passed"
