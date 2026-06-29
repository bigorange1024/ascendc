#!/bin/bash
# fix-f203-2s1e-alg13-16171820-vec-k4
# host 1s+1e → NTT + 行18–20 UB 融合（平面 mat_c，无 Gather）
#
# Usage:
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#
# 环境变量：
#   NTTS2S1E_MIX_PASS / TAG5T_MIX_PASS — 0=全链路（默认）
#   F203_STAGE1_SPLIT — 0 标量 / 1 bulk向量（默认）/ 2 tile32
#   F203_MOD_VARIANT — 设备行18 mod（golden 仍标量）
# CPU mixPass=0 失败时自动 5→4 两段 + checkpoint（见 verify_result.py）
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)
REPO_ROOT="$(cd "${CURRENT_DIR}/../../.." && pwd)"

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
export NTTS2S1E_MIX_PASS="${NTTS2S1E_MIX_PASS:-0}"
export F203_STAGE1_SPLIT="${F203_STAGE1_SPLIT:-1}"
export HAT_ALG11_VEC="${HAT_ALG11_VEC:-1}"
export BYTE_ENCODE12_VEC="${BYTE_ENCODE12_VEC:-1}"
export BYTE_ENCODE12_SCATTER_VEC="${BYTE_ENCODE12_SCATTER_VEC:-1}"
export ALG11_IMPL="${ALG11_IMPL:-1}"
export ALG11_VEC_VARIANT="${ALG11_VEC_VARIANT:-2}"
export ALG11_VEC_OPTS="${ALG11_VEC_OPTS:-0}"
export ALG11_MEM_OPS="${ALG11_MEM_OPS:-1}"
echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} NTTS2S1E_MIX_PASS=${NTTS2S1E_MIX_PASS} F203_STAGE1_SPLIT=${F203_STAGE1_SPLIT} HAT_ALG11_VEC=${HAT_ALG11_VEC} BYTE_ENCODE12_VEC=${BYTE_ENCODE12_VEC} ALG11_IMPL=${ALG11_IMPL} ALG11_VEC_VARIANT=${ALG11_VEC_VARIANT} ALG11_MEM_OPS=${ALG11_MEM_OPS}"

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
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
    -DF203_STAGE1_SPLIT=${F203_STAGE1_SPLIT} \
    -DHAT_ALG11_VEC=${HAT_ALG11_VEC} \
    -DBYTE_ENCODE12_VEC=${BYTE_ENCODE12_VEC} \
    -DBYTE_ENCODE12_SCATTER_VEC=${BYTE_ENCODE12_SCATTER_VEC} \
    -DALG11_IMPL=${ALG11_IMPL} \
    -DALG11_VEC_VARIANT=${ALG11_VEC_VARIANT} \
    -DALG11_VEC_OPTS=${ALG11_VEC_OPTS} \
    -DALG11_MEM_OPS=${ALG11_MEM_OPS}
cmake --build build -j
cmake --install build

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output
mkdir -p input output
python3 "${CURRENT_DIR}/scripts/gen_data.py"

if [ "${NTTS2S1E_MIX_PASS}" = "2" ]; then
    if [ -f "./output/s0.bin" ]; then
        cp ./output/s0.bin ./input/s0_preset.bin
    elif [ -f "./output/golden_s0.bin" ]; then
        cp ./output/golden_s0.bin ./input/s0_preset.bin
    fi
fi
if [ "${NTTS2S1E_MIX_PASS}" = "3" ]; then
    if [ -f "./output/mat_c.bin" ]; then
        cp ./output/mat_c.bin ./input/mat_c_preset.bin
    elif [ -f "./output/golden_mat_c.bin" ]; then
        cp ./output/golden_mat_c.bin ./input/mat_c_preset.bin
    fi
fi
if [ "${NTTS2S1E_MIX_PASS}" = "4" ] || [ "${NTTS2S1E_MIX_PASS}" = "7" ]; then
    if [ -f "./output/dst.bin" ]; then
        cp ./output/dst.bin ./input/dst_preset.bin
    elif [ -f "./output/golden.bin" ]; then
        cp ./output/golden.bin ./input/dst_preset.bin
    fi
fi
if [ "${NTTS2S1E_MIX_PASS}" = "7" ]; then
    if [ -f "./output/golden_t_hat.bin" ]; then
        cp ./output/golden_t_hat.bin ./input/t_hat_preset.bin
    elif [ -f "./output/t_hat.bin" ]; then
        cp ./output/t_hat.bin ./input/t_hat_preset.bin
    fi
fi

export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH

if [ "${RUN_MODE}" = "sim" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-120}"
else
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-60}"
fi
if [ "${RUN_MODE}" = "sim" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi
if [ "${NTTS2S1E_MIX_PASS}" = "0" ] && [ "${RUN_MODE}" = "cpu" ]; then
    export NTTS2S1E_MIX_PASS=5
    python3 "${CURRENT_DIR}/scripts/gen_data.py"
    bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
    cp ./output/dst.bin ./input/dst_preset.bin
    cp ./output/s0.bin ./output/_checkpoint_s0.bin
    cp ./output/mat_c.bin ./output/_checkpoint_mat_c.bin
    export NTTS2S1E_MIX_PASS=4
    python3 "${CURRENT_DIR}/scripts/gen_data.py"
    bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
else
    bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
fi
if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

python3 "${CURRENT_DIR}/scripts/verify_result.py"
echo "[SUCCESS] fix-f203-2s1e-alg13-16171820-vec-k4 (${RUN_MODE})"
