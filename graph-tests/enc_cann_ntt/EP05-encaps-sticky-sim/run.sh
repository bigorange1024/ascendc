#!/bin/bash
# EP05-encaps-sticky-sim：EN13 Encrypt×liboqs × 粘性多轮（同 session）
# 基线：复制 EN13 核；编排对齐 EN12 sticky；默认 R=16（EP05_ROUNDS）。
# 允许 -r npu（2026-09-12 用户授权）。主门禁：R 轮不挂 + 每轮 c/K≡liboqs encaps max=0。
#
# Usage（默认 = sticky R=16）：
#   EP05_ROUNDS=16 bash run.sh -r cpu -v Ascend910B4
#   EP05_ROUNDS=16 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#
# 调试（非默认）：
#   EP05_ROUNDS=2 bash run.sh -r cpu -v Ascend910B4
#
# KERNEL_COMPUTE_BUDGET_SEC：CPU 默认 1800；SIM 默认 7200（R=16×~120s）
# 已锁：NTT_N=256 NTT_Q=3329 NTT_REF=kyber NTT_BENCH=4；K=4；du=11 dv=5
# 抽样交叉（非默认）：EP05_CROSS_STRIDE=4
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

# NPU unlocked 2026-09-12 (user authorized)

# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/runtime_env.sh"
export ASCENDC_CASE_SUPPORTS_NPU=1
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
export EP05_ROUNDS="${EP05_ROUNDS:-16}"
echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} CANN=${_ASCEND_INSTALL_PATH} ASCEND_DEVICE_ID=${ASCEND_DEVICE_ID:-n/a} EP05_ROUNDS=${EP05_ROUNDS}"

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
# 预建每轮输出目录（WriteFile 不 mkdir）
for ((_r=1; _r<=EP05_ROUNDS; _r++)); do
    mkdir -p "$(printf "output/r%02d" "${_r}")"
done

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
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-7200}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
else
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-1800}"
fi
bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
_rc=$?
if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi
if [ "${_rc}" -ne 0 ]; then
    echo "[FAIL] kernel exit=${_rc} (${RUN_MODE}) — sticky R=${EP05_ROUNDS} hang or error" >&2
    exit "${_rc}"
fi

_missing=0
for ((_r=1; _r<=EP05_ROUNDS; _r++)); do
    _f=$(printf "output/round_%02d_done.txt" "${_r}")
    if [ ! -f "${_f}" ]; then
        echo "[FAIL] missing ${_f}" >&2
        _missing=1
    else
        echo "[EP05] found ${_f}: $(tr -d '\n' < "${_f}")"
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
    echo "[FAIL] EP05 sticky c vs liboqs 对拍未过（${RUN_MODE}）R=${EP05_ROUNDS}" >&2
    exit "${_vr}"
fi
echo "[SUCCESS] EP05-encaps-sticky-sim (${RUN_MODE}) R=${EP05_ROUNDS} exit=0 + each-round c/K≡liboqs encaps max=0"
exit 0
