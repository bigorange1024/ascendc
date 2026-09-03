#!/bin/bash
# fix-encrypt-skel-mix-chain-toy：Encrypt 任务链骨架 toy（stub hash/NTT/inner/[GATE]/INTT/encode）
#
# Usage（默认 = GATE 开 + HEAVY/SKIPNTT 关）：
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#   SKEL_SKIPNTT=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4   # A 基线
#
# TASK-004/005 skipNtt（须显式）：
#   SKEL_SKIPNTT=1 SKEL_HOST_MU=1 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4  # Host 折 μ
#   SKEL_SKIPNTT=1 SKEL_HOST_MU=0 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4  # 设备 μ-stub
#   # 故障注入（预期 timeout 124；可降预算）：
#   KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_SKIPNTT=1 SKEL_HOST_MU=1 SKEL_OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#
# 其它对照（非默认）：
#   SKEL_GATE=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4   # 无 GATE
#   SKEL_HEAVY=1 SKEL_GATE=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#   SKEL_SKIPNTT=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4   # A 基线（非 skipNtt）
#
# KERNEL_COMPUTE_BUDGET_SEC 默认 180（防挂死，非性能定标）
# CANN：source ${REPO_ROOT}/scripts/env.sh
# SKEL_*：编译期宏（cmake -D）；verify 读同一 env 校验 out[8]
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)
_ORIG_ARGS=("$@")
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
    echo "[ERROR] CANN_HOME / ASCEND_HOME_PATH 未设置；请检查 scripts/env.sh" >&2
    exit 1
fi
if ! command -v ccec >/dev/null 2>&1; then
    echo "[ERROR] 未找到 ccec。CANN_HOME=${CANN_HOME:-} install=${_ASCEND_INSTALL_PATH}" >&2
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
echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} CANN=${_ASCEND_INSTALL_PATH} ASCEND_DEVICE_ID=${ASCEND_DEVICE_ID:-n/a}"

if [ "${RUN_MODE}" = "sim" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
elif [ "${RUN_MODE}" = "npu" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH
fi

# SKEL_GATE 默认 1；0=无 GATE（对照；仅 SKIPNTT=0 时生效）
export SKEL_GATE="${SKEL_GATE:-1}"
if [ "${SKEL_GATE}" != "0" ] && [ "${SKEL_GATE}" != "1" ]; then
    echo "[ERROR] SKEL_GATE must be 0 or 1, got: ${SKEL_GATE}" >&2
    exit 1
fi
# SKEL_HEAVY 默认 0（接近基线）；1=16×64×64 + 4 轮 Cube
export SKEL_HEAVY="${SKEL_HEAVY:-0}"
if [ "${SKEL_HEAVY}" != "0" ] && [ "${SKEL_HEAVY}" != "1" ]; then
    echo "[ERROR] SKEL_HEAVY must be 0 or 1, got: ${SKEL_HEAVY}" >&2
    exit 1
fi
# SKEL_SKIPNTT 默认 0；1=AIC 入口 Wait(4)
export SKEL_SKIPNTT="${SKEL_SKIPNTT:-0}"
if [ "${SKEL_SKIPNTT}" != "0" ] && [ "${SKEL_SKIPNTT}" != "1" ]; then
    echo "[ERROR] SKEL_SKIPNTT must be 0 or 1, got: ${SKEL_SKIPNTT}" >&2
    exit 1
fi
# SKEL_OMIT_SET4 默认 0；1=故障注入不 SET(4)（仅与 SKIPNTT=1 联用）
export SKEL_OMIT_SET4="${SKEL_OMIT_SET4:-0}"
if [ "${SKEL_OMIT_SET4}" != "0" ] && [ "${SKEL_OMIT_SET4}" != "1" ]; then
    echo "[ERROR] SKEL_OMIT_SET4 must be 0 or 1, got: ${SKEL_OMIT_SET4}" >&2
    exit 1
fi
if [ "${SKEL_OMIT_SET4}" = "1" ] && [ "${SKEL_SKIPNTT}" != "1" ]; then
    echo "[ERROR] SKEL_OMIT_SET4=1 requires SKEL_SKIPNTT=1" >&2
    exit 1
fi
# SKEL_HOST_MU 默认 1（TASK-005 默认测 Host 折 μ）；0=设备 μ-stub；仅与 SKIPNTT=1 联用才有语义
export SKEL_HOST_MU="${SKEL_HOST_MU:-1}"
if [ "${SKEL_HOST_MU}" != "0" ] && [ "${SKEL_HOST_MU}" != "1" ]; then
    echo "[ERROR] SKEL_HOST_MU must be 0 or 1, got: ${SKEL_HOST_MU}" >&2
    exit 1
fi
if [ "${SKEL_HOST_MU}" = "1" ] && [ "${SKEL_SKIPNTT}" != "1" ]; then
    # Host 折 μ 仅 skipNtt 路径有意义；非 skipNtt 时允许=1 但无效果（宏在 #if SKIPNTT 内）
    :
fi
echo "SKEL_GATE=${SKEL_GATE} SKEL_HEAVY=${SKEL_HEAVY} SKEL_SKIPNTT=${SKEL_SKIPNTT} SKEL_OMIT_SET4=${SKEL_OMIT_SET4} SKEL_HOST_MU=${SKEL_HOST_MU}"

set -e
rm -rf build out
mkdir -p build
cmake -B build \
    -DRUN_MODE=${RUN_MODE} \
    -DSOC_VERSION=${SOC_VERSION} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
    -DASCEND_CANN_PACKAGE_PATH=${_ASCEND_INSTALL_PATH} \
    -DSKEL_GATE=${SKEL_GATE} \
    -DSKEL_HEAVY=${SKEL_HEAVY} \
    -DSKEL_SKIPNTT=${SKEL_SKIPNTT} \
    -DSKEL_OMIT_SET4=${SKEL_OMIT_SET4} \
    -DSKEL_HOST_MU=${SKEL_HOST_MU}
cmake --build build -j"${CMAKE_BUILD_JOBS:-2}"
cmake --install build

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output
mkdir -p input output
python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH

if [ "${RUN_MODE}" = "sim" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-180}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
else
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-30}"
fi
bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

python3 "${CURRENT_DIR}/scripts/verify_result.py"
echo "[SUCCESS] fix-encrypt-skel-mix-chain-toy (${RUN_MODE})"
