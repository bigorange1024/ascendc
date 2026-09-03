#!/bin/bash
# fix-decrypt-skel-mix-chain-toy：Decrypt fused 握手骨架 toy（SoftSync + 两轮 GATE + stub Cube）
#
# Usage（默认 = 合法握手，OMIT_*=0，SOFTSYNC_PREFILL=0）：
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#   # 等价显式：
#   SKEL_SOFTSYNC_PREFILL=0 SKEL_OMIT_SET4_R2=0 SKEL_OMIT_SET4=0 SKEL_OMIT_SLOT0=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#
# Host softSync 预填（运行时 env，非 cmake；TASK-012 / J-dirty-softsync-hang-vs-race；两档均预期绿）：
#   SKEL_SOFTSYNC_PREFILL=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4   # 清零（默认）
#   SKEL_SOFTSYNC_PREFILL=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4   # int32[2] 都写 1（脏误放行≠hang）
#
# 故障注入（预期 timeout 124；可降预算；OMIT_SET4 / OMIT_SLOT0 / OMIT_SET4_R2 三者互斥）：
#   KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#   KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SLOT0=1 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#   KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SET4_R2=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#
# KERNEL_COMPUTE_BUDGET_SEC 默认 180（防挂死，非性能定标）
# CANN：source ${REPO_ROOT}/scripts/env.sh
# SKEL_OMIT_*：编译期宏（cmake -D）；同时开多个则报错退出
# SKEL_SOFTSYNC_PREFILL：仅 Host 运行时；勿与 OMIT_* 叠开测 hang
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

# SKEL_OMIT_SET4 默认 0；1=故障注入不 SET(4)（两轮 GATE）
export SKEL_OMIT_SET4="${SKEL_OMIT_SET4:-0}"
if [ "${SKEL_OMIT_SET4}" != "0" ] && [ "${SKEL_OMIT_SET4}" != "1" ]; then
    echo "[ERROR] SKEL_OMIT_SET4 must be 0 or 1, got: ${SKEL_OMIT_SET4}" >&2
    exit 1
fi
# SKEL_OMIT_SLOT0 默认 0；1=AIV0 不写 SoftSync slot0（SET(4) 前置断裂）
export SKEL_OMIT_SLOT0="${SKEL_OMIT_SLOT0:-0}"
if [ "${SKEL_OMIT_SLOT0}" != "0" ] && [ "${SKEL_OMIT_SLOT0}" != "1" ]; then
    echo "[ERROR] SKEL_OMIT_SLOT0 must be 0 or 1, got: ${SKEL_OMIT_SLOT0}" >&2
    exit 1
fi
# SKEL_OMIT_SET4_R2 默认 0；1=仅第二轮 GATE(slot1) 不 SET(4)
export SKEL_OMIT_SET4_R2="${SKEL_OMIT_SET4_R2:-0}"
if [ "${SKEL_OMIT_SET4_R2}" != "0" ] && [ "${SKEL_OMIT_SET4_R2}" != "1" ]; then
    echo "[ERROR] SKEL_OMIT_SET4_R2 must be 0 or 1, got: ${SKEL_OMIT_SET4_R2}" >&2
    exit 1
fi
# 三者互斥：同时为 1 的开关数不得超过 1
_OMIT_ON=0
[ "${SKEL_OMIT_SET4}" = "1" ] && _OMIT_ON=$((_OMIT_ON + 1))
[ "${SKEL_OMIT_SLOT0}" = "1" ] && _OMIT_ON=$((_OMIT_ON + 1))
[ "${SKEL_OMIT_SET4_R2}" = "1" ] && _OMIT_ON=$((_OMIT_ON + 1))
if [ "${_OMIT_ON}" -gt 1 ]; then
    echo "[ERROR] SKEL_OMIT_SET4 / SKEL_OMIT_SLOT0 / SKEL_OMIT_SET4_R2 互斥；同时为 1 的不得超过一个（got SET4=${SKEL_OMIT_SET4} SLOT0=${SKEL_OMIT_SLOT0} SET4_R2=${SKEL_OMIT_SET4_R2}）" >&2
    exit 1
fi
# SKEL_SOFTSYNC_PREFILL：Host 运行时；0=清零（默认）；1=脏写 int32[2]=1（预期仍绿，非 hang）
export SKEL_SOFTSYNC_PREFILL="${SKEL_SOFTSYNC_PREFILL:-0}"
if [ "${SKEL_SOFTSYNC_PREFILL}" != "0" ] && [ "${SKEL_SOFTSYNC_PREFILL}" != "1" ]; then
    echo "[ERROR] SKEL_SOFTSYNC_PREFILL must be 0 or 1, got: ${SKEL_SOFTSYNC_PREFILL}" >&2
    exit 1
fi
echo "SKEL_OMIT_SET4=${SKEL_OMIT_SET4} SKEL_OMIT_SLOT0=${SKEL_OMIT_SLOT0} SKEL_OMIT_SET4_R2=${SKEL_OMIT_SET4_R2} SKEL_SOFTSYNC_PREFILL=${SKEL_SOFTSYNC_PREFILL}"

set -e
rm -rf build out
mkdir -p build
cmake -B build \
    -DRUN_MODE=${RUN_MODE} \
    -DSOC_VERSION=${SOC_VERSION} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
    -DASCEND_CANN_PACKAGE_PATH=${_ASCEND_INSTALL_PATH} \
    -DSKEL_OMIT_SET4=${SKEL_OMIT_SET4} \
    -DSKEL_OMIT_SLOT0=${SKEL_OMIT_SLOT0} \
    -DSKEL_OMIT_SET4_R2=${SKEL_OMIT_SET4_R2}
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
echo "[SUCCESS] fix-decrypt-skel-mix-chain-toy (${RUN_MODE})"
