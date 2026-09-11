#!/bin/bash
# EN12-samplentt-sticky：EN09 SampleNTT 贯通六段 × 粘性多轮（同 session 不 recreate stream）
# 基线：复制 EN09；默认 R=16（EN12_ROUNDS）；第 1 轮 golden；Â 必须设备 SampleNTT。
# 允许 -r npu（ASCENDC_CASE_SUPPORTS_NPU=1；默认 ASCEND_DEVICE_ID=4，可被 env 覆盖）。
# 禁融胖 MIX GATE；禁抄 Encrypt/KEM/frozen/ER 核。
#
# Usage（默认 = Kyber 档 + sticky R=16；CPU/SIM 验收；NPU 由主控上机）：
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#   ASCEND_DEVICE_ID=4 bash run.sh -r npu -v Ascend910B3   # 真机；本 subagent 不跑
#
# 调试（非默认）：
#   EN12_ROUNDS=2 bash run.sh -r cpu -v Ascend910B4
#
# 主门禁：R 轮全部完成不挂。golden 对拍仅第 1 轮；失败仅标 CORRECTNESS_SOFT_FAIL。
# KERNEL_COMPUTE_BUDGET_SEC：CPU 默认 1200；SIM 默认 7200（R=16×SampleNTT 防挂死）
# 已锁：NTT_N=256 NTT_Q=3329 NTT_REF=kyber NTT_BENCH=4；K=4；du=11 dv=5
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

# 本刀允许 NPU：真机默认卡 4（EN10/KB X39）；显式 export 优先；SIM 仍由 runtime_env 强制 0
export ASCENDC_CASE_SUPPORTS_NPU=1
if [ "${RUN_MODE}" = "npu" ]; then
    export ASCEND_DEVICE_ID="${ASCEND_DEVICE_ID:-4}"
fi

# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/runtime_env.sh"
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
if [ "${RUN_MODE}" = "sim" ]; then
    export ASCEND_DEVICE_ID=0
fi
export EN12_ROUNDS="${EN12_ROUNDS:-16}"
echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} CANN=${_ASCEND_INSTALL_PATH} ASCEND_DEVICE_ID=${ASCEND_DEVICE_ID:-n/a} EN12_ROUNDS=${EN12_ROUNDS}"

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
    -DASCEND_CANN_PACKAGE_PATH=${_ASCEND_INSTALL_PATH}
cmake --build build -j"${CMAKE_BUILD_JOBS:-2}"
cmake --install build

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output
mkdir -p input output

export NTT_N="${NTT_N:-256}"
export NTT_BENCH="${NTT_BENCH:-4}"
export NTT_Q="${NTT_Q:-3329}"
export NTT_REF="${NTT_REF:-kyber}"
export SEED_D="${SEED_D:-20260619}"
export PREP_NONCE0="${PREP_NONCE0:-0}"
export FIPS203_PRF_BACKEND=shake256
python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH

if [ "${RUN_MODE}" = "sim" ]; then
    # R=16 × SampleNTT 贯通链：EN09 单轮 SIM≈129s → 估 ~35min；给足防挂死预算
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-7200}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
elif [ "${RUN_MODE}" = "npu" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-1800}"
else
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-1200}"
fi
bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
_rc=$?
if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi
if [ "${_rc}" -ne 0 ]; then
    echo "[FAIL] kernel exit=${_rc} (${RUN_MODE}) — sticky R=${EN12_ROUNDS} hang or error" >&2
    exit "${_rc}"
fi

# 确认 R 轮完成标记齐全（主门禁补充证据）
_missing=0
for ((_r=1; _r<=EN12_ROUNDS; _r++)); do
    _f=$(printf "output/round_%02d_done.txt" "${_r}")
    if [ ! -f "${_f}" ]; then
        echo "[FAIL] missing ${_f}" >&2
        _missing=1
    else
        echo "[EN12] found ${_f}: $(tr -d '\n' < "${_f}")"
    fi
done
if [ "${_missing}" -ne 0 ]; then
    exit 61
fi

set +e
python3 "${CURRENT_DIR}/scripts/verify_result.py"
_vr=$?
set -e
if [ "${_vr}" -ne 0 ]; then
    echo "[CORRECTNESS_SOFT_FAIL] round-1 SampleNTT+wired golden 对拍未过（${RUN_MODE}）；R=${EN12_ROUNDS} kernel 已不挂 exit=0"
else
    echo "[SUCCESS] EN12-samplentt-sticky (${RUN_MODE}) R=${EN12_ROUNDS} exit=0 + round-1 golden match"
fi
exit 0
