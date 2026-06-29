#!/bin/bash
# Phase A：6bit encode + FSM CrossCore（MIX），对拍 mat_a golden
# Usage: bash run.sh -r cpu -v Ascend910B4
#        SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)
SIM_DIRECT="${SIM_DIRECT:-1}"
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
elif [ -d "$HOME/Ascend/cann" ]; then
    _ASCEND_INSTALL_PATH=$HOME/Ascend/cann
else
    _ASCEND_INSTALL_PATH=/usr/local/Ascend/ascend-toolkit/latest
fi

echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} SIM_DIRECT=${SIM_DIRECT} (Phase A FSM encode)"

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
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

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output && mkdir -p input output
python3 scripts/gen_data.py
export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH

if [ "${RUN_MODE}" = "sim" ] && [ "${SIM_DIRECT}" != "1" ]; then
    echo "ERROR: Phase A 请用 SIM_DIRECT=1" >&2
    exit 1
fi
KERNEL_TIMEOUT_SEC="${KERNEL_TIMEOUT_SEC:-150}"
if ! timeout "${KERNEL_TIMEOUT_SEC}" ./ascendc_kernels_bbit; then
    rc=$?
    if [ "$rc" -eq 124 ]; then
        echo "[FAILED] kernel exceeded ${KERNEL_TIMEOUT_SEC}s (FSM deadlock?)"
        exit 124
    fi
    exit "$rc"
fi

if [ "${RUN_MODE}" != "cpu" ]; then
    python3 -c "import struct; v=struct.unpack('<i', open('output/sync.bin','rb').read())[0]; assert v==0xA1C0A1C0, hex(v)"
    echo "[check] AIC FSM sync marker OK (0xa1c0a1c0)"
fi

md5sum output/mat_a_gm.bin output/golden.bin
python3 scripts/verify_result.py output/mat_a_gm.bin output/golden.bin
echo "[SUCCESS] Phase A f203-ntt-phase-a-fsm (${RUN_MODE})"
