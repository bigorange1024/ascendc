#!/bin/bash
# exp-mlkem-f203-stage1-encode-vec: 纯向量 Stage1（cpu / sim / npu）
# Usage:
#   bash run.sh -r cpu -v Ascend910B4                  # 默认 aiv=1 + aiv=2 + aiv=8
#   bash run.sh -r cpu -v Ascend910B4 --aiv 2
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)

BUILD_TYPE="Debug"
INSTALL_PREFIX="${CURRENT_DIR}/out"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"
LAUNCH_PROFILE=""

SHORT=r:,v:,i:,b:,p:,P:
LONG=run-mode:,soc-version:,install-path:,build-type:,install-prefix:,profile:,aiv:
OPTS=$(getopt -a --options $SHORT --longoptions $LONG -- "$@")
eval set -- "$OPTS"

while :; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    -i | --install-path) ASCEND_INSTALL_PATH="$2"; shift 2 ;;
    -b | --build-type) BUILD_TYPE="$2"; shift 2 ;;
    -p | --install-prefix) INSTALL_PREFIX="$2"; shift 2 ;;
    -P | --profile) LAUNCH_PROFILE="$2"; shift 2 ;;
    --aiv) LAUNCH_PROFILE="aiv=$2"; shift 2 ;;
    --) shift; break ;;
    *) echo "[ERROR] Unexpected option: $1"; exit 1 ;;
    esac
done

if [[ -n "$LAUNCH_PROFILE" && "$LAUNCH_PROFILE" != "aiv=1" && "$LAUNCH_PROFILE" != "aiv=2" && "$LAUNCH_PROFILE" != "aiv=8" ]]; then
    echo "ERROR: LAUNCH_PROFILE must be aiv=1, aiv=2, or aiv=8"
    exit 1
fi

if [ -n "$LAUNCH_PROFILE" ]; then
    PROFILES=("$LAUNCH_PROFILE")
else
    PROFILES=(aiv=1 aiv=2 aiv=8)
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
echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} PROFILES=${PROFILES[*]}"

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

compare_golden() {
    local profile="$1"
    echo "=== Compare output vs golden (${profile}) ==="
    md5sum output/mat_a_gm.bin output/golden.bin
    if ! cmp -s output/mat_a_gm.bin output/golden.bin; then
        echo "[FAILED] output differs from golden (profile=${profile})"
        exit 1
    fi
    python3 scripts/verify_result.py output/mat_a_gm.bin output/golden.bin
}

for profile in "${PROFILES[@]}"; do
    echo "=== Launch profile: ${profile} ==="
    export LAUNCH_PROFILE="${profile}"
    (
        export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH
        ./ascendc_kernels_bbit
    )
    compare_golden "${profile}"
done

echo "[SUCCESS] all profiles passed: ${PROFILES[*]}"
