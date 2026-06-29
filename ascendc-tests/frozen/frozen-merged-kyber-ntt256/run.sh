#!/bin/bash
# Phase D：第三方 merged_kyber 单 poly N=256 Kyber NTT（7bit split + 2×Mmad + Merge）
# 源码：thirdparty/merged_kyber/（本目录仅 harness）
# Usage:
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4   # 默认走 msprof op simulator → OPPROF_*
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)
MERGED_KYBER_ROOT="${CURRENT_DIR}/../../../thirdparty/merged_kyber"
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

RUN_MODE_LIST="cpu sim npu"
if [[ " $RUN_MODE_LIST " != *" $RUN_MODE "* ]]; then
    echo "ERROR: RUN_MODE must be one of: cpu sim npu"
    exit 1
fi

VERSION_LIST="Ascend310P1 Ascend310P3 Ascend910B1 Ascend910B2 Ascend910B3 Ascend910B4"
if [[ " $VERSION_LIST " != *" $SOC_VERSION "* ]]; then
    echo "ERROR: SOC_VERSION should be in [$VERSION_LIST]"
    exit 1
fi

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
echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} SIM_DIRECT=${SIM_DIRECT} (Phase D merged_kyber)"

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
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
cmake -B build \
    -DRUN_MODE=${RUN_MODE} \
    -DSOC_VERSION=${SOC_VERSION} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
    -DASCEND_CANN_PACKAGE_PATH=${_ASCEND_INSTALL_PATH}
cmake --build build -j
cmake --install build

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output
mkdir -p input output
python3 "${MERGED_KYBER_ROOT}/scripts/gen_data.py"
export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH

if [ "${RUN_MODE}" = "cpu" ]; then
    ./ascendc_kernels_bbit
elif [ "${RUN_MODE}" = "sim" ] && [ "${SIM_DIRECT}" = "1" ]; then
    echo "[run.sh] SIM_DIRECT=1 → 仅 CAModel golden，不跑 msprof"
    ./ascendc_kernels_bbit
elif [ "${RUN_MODE}" = "sim" ]; then
    require_msprof() {
        command -v msprof >/dev/null 2>&1 || {
            echo "ERROR: msprof not found; use SIM_DIRECT=1 for fast sim" >&2
            exit 1
        }
    }
    require_msprof
    prof_out="${CURRENT_DIR}/prof_sim"
    rm -rf "${prof_out}"
    mkdir -p "${prof_out}"
    msprof op simulator \
        --soc-version="${SOC_VERSION}" \
        --output="${prof_out}" \
        --application="${CURRENT_DIR}/ascendc_kernels_bbit" \
        --aic-metrics=PipeUtilization \
        --launch-count=1 \
        --timeout=60
    opprof_dir="$(ls -dt "${CURRENT_DIR}"/OPPROF_* 2>/dev/null | head -1 || true)"
    if [ -n "${opprof_dir}" ]; then
        chmod -R u+rwX,go+rX "${opprof_dir}" 2>/dev/null || true
        ln -sfn "${opprof_dir}" "${prof_out}/latest" 2>/dev/null || true
        echo "[INFO] OPPROF: ${opprof_dir}"
    fi
else
    ./ascendc_kernels_bbit
fi

md5sum output/dst.bin output/golden.bin
python3 "${MERGED_KYBER_ROOT}/scripts/verify_result.py" output/dst.bin output/golden.bin
echo "[SUCCESS] Phase D merged-kyber-ntt256 (${RUN_MODE})"
