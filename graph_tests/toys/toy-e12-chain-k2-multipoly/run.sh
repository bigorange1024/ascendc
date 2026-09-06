#!/bin/bash
# toy-e12-chain-k2-multipoly：E11 壳扩 k=2 — SHAKE+CBD×2 + L2 真链×2 + SET4 ×3
#
# Usage（默认 = 3 轮 SIM；真 SHAKE+CBD×2+NTT+basemul+INTT+Decompress_1(μ)+Compress+ByteEncode×2+SET4）：
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#
# 调试（非默认）：
#   TOY_ROUNDS=5 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#   TOY_SKIP_GOLDEN=1 …   # 仅 TRACE/不挂，跳过 golden
#
# KERNEL_COMPUTE_BUDGET_SEC 默认 1200（3×k=2 真链；防挂死，非性能定标）
# Decompress_1=消息嵌入 d=1（μ 32B 共享）；Compress/ByteEncode d=4（2×128B=256B）
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)
_ORIG_ARGS=("$@")
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

# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/runtime_env.sh"
export ASCENDC_CASE_SUPPORTS_NPU="${ASCENDC_CASE_SUPPORTS_NPU:-1}"
runtime_env_dispatch "${BASH_SOURCE[0]}" "${_ORIG_ARGS[@]}"

set +e
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/env.sh"
_env_rc=$?
set -e
if [ "${_env_rc}" -ne 0 ]; then
    echo "[ERROR] source ${REPO_ROOT}/scripts/env.sh failed (rc=${_env_rc})" >&2
    exit 1
fi
if [ -n "${ASCEND_INSTALL_PATH:-}" ]; then
    _ASCEND_INSTALL_PATH="${ASCEND_INSTALL_PATH}"
elif [ -n "${CANN_HOME:-}" ] && [ -d "${CANN_HOME}" ]; then
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
elif [ -n "${ASCEND_HOME_PATH:-}" ] && [ -d "${ASCEND_HOME_PATH}" ]; then
    _ASCEND_INSTALL_PATH="${ASCEND_HOME_PATH}"
else
    echo "[ERROR] CANN_HOME / ASCEND_HOME_PATH 未设置" >&2
    exit 1
fi
if ! command -v ccec >/dev/null 2>&1; then
    echo "[ERROR] 未找到 ccec" >&2
    exit 1
fi

export ASCEND_TOOLKIT_HOME="${_ASCEND_INSTALL_PATH}"
export ASCEND_HOME_PATH="${_ASCEND_INSTALL_PATH}"
export ASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}"
if [ "${RUN_MODE}" = "npu" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/npu_device_map.sh"
    npu_device_map_apply "${CURRENT_DIR}"
elif [ "${RUN_MODE}" = "sim" ]; then
    export ASCEND_DEVICE_ID=0
fi

export TOY_ROUNDS="${TOY_ROUNDS:-3}"
export F203_BYTE_ENCODE_D="${F203_BYTE_ENCODE_D:-4}"
export BYTE_ENCODE_D_VEC="${BYTE_ENCODE_D_VEC:-1}"
export F203_DECOMPRESS_D="${F203_DECOMPRESS_D:-1}"
echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} CANN=${_ASCEND_INSTALL_PATH} TOY_ROUNDS=${TOY_ROUNDS} F203_DECOMPRESS_D=${F203_DECOMPRESS_D} F203_BYTE_ENCODE_D=${F203_BYTE_ENCODE_D}"

if [ "${RUN_MODE}" = "sim" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
elif [ "${RUN_MODE}" = "npu" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH
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
    -DF203_BYTE_ENCODE_D=${F203_BYTE_ENCODE_D} \
    -DBYTE_ENCODE_D_VEC=${BYTE_ENCODE_D_VEC} \
    -DF203_DECOMPRESS_D=${F203_DECOMPRESS_D}
cmake --build build -j"${CMAKE_BUILD_JOBS:-2}"
cmake --install build

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output
mkdir -p input output
python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH

if [ "${RUN_MODE}" = "sim" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-1200}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
else
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-120}"
fi

set +e
bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit 2>&1 | tee "${CURRENT_DIR}/output/host_trace.log"
_rc=${PIPESTATUS[0]}
set -e
if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

if [ "${_rc}" -ne 0 ]; then
    echo "[FAIL] kernel rc=${_rc}" >&2
    exit "${_rc}"
fi

python3 "${CURRENT_DIR}/scripts/verify_result.py"
echo "[SUCCESS] toy-e12-chain-k2-multipoly (${RUN_MODE})"
