#!/bin/bash
# =============================================================================
# pass-shake128-ops-math-toy — 共享 shake_xof_kernel（rate=168）+ tiny_sha3/Python 对拍
# =============================================================================
#
# Usage（默认）：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#   bash run.sh -r npu -v Ascend910B4
#
# 环境变量：
#   SHAKE128_CASE — abc（默认）| empty | prf_sigma_n0 | batch_mixed
#   SEED_D        — prf_sigma_n0 用例的 derand 种子（默认 20260619）
# =============================================================================
#
# 多环境分流（scripts/runtime_env.sh）：
#   bash run.sh -r auto -v Ascend910B4      # 单档最优 npu>sim>cpu（≠完整验收）
#   bash run.sh -r verify -v Ascend910B4    # cpu → SIM_DIRECT sim [→ npu，非WSL]
#   WSL 禁止 -r npu；说明见 docs/engineering/NPU真机环境说明.md
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)
_ORIG_ARGS=("$@")
REPO_ROOT="$(cd "${CURRENT_DIR}/../.." && pwd)"

SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"
CCEC_ARCH="dav-c220"

SHORT=r:,v:,i:
LONG=run-mode:,soc-version:,install-path:
OPTS=$(getopt -a --options $SHORT --longoptions $LONG -- "$@")
eval set -- "$OPTS"

while :; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    -i | --install-path) ASCEND_INSTALL_PATH="$2"; shift 2 ;;
    --) shift; break ;;
    *) echo "[ERROR] Unexpected option: $1"; exit 1 ;;
    esac
done


# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/runtime_env.sh"
export ASCENDC_CASE_SUPPORTS_NPU="${ASCENDC_CASE_SUPPORTS_NPU:-1}"
runtime_env_dispatch "${BASH_SOURCE[0]}" "${_ORIG_ARGS[@]}"
# Host golden 需要 tiny_sha3
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/ensure_thirdparty_dep.sh"
ensure_thirdparty_dep tiny_sha3 sha3.c
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
export SHAKE128_CASE="${SHAKE128_CASE:-abc}"
export SEED_D="${SEED_D:-20260619}"

echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} SHAKE128_CASE=${SHAKE128_CASE} SEED_D=${SEED_D}"

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
fi

FILE_NAME="shake128_ops_math"
SIM_LIB="${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib"
if [ ! -d "${SIM_LIB}" ]; then
    SIM_LIB="${_ASCEND_INSTALL_PATH}/toolkit/tools/simulator/${SOC_VERSION}/lib"
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export LD_LIBRARY_PATH=${SIM_LIB}:${LD_LIBRARY_PATH:-}
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${SIM_LIB}:${LD_LIBRARY_PATH:-}
elif [ "${RUN_MODE}" = "npu" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}
fi

set -e
rm -rf build input output
mkdir -p input output
python3 "${CURRENT_DIR}/gen_data.py"

cmake -S "${CURRENT_DIR}" -B build \
    -Dproduct_type="${SOC_VERSION}" \
    -Dsim_device="${SOC_VERSION}" \
    -Dtikicpu_lib_device="${SOC_VERSION}" \
    -Dccec_aicore_arch="${CCEC_ARCH}" \
    -Drun_mode="${RUN_MODE}" \
    -Dinstall_path="${_ASCEND_INSTALL_PATH}"
cmake --build build -j

if [ "${RUN_MODE}" = "sim" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-120}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi

case "${RUN_MODE}" in
cpu)
  "./${FILE_NAME}_cpu"
  ;;
sim | npu)
  bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "./${FILE_NAME}_npu"
  ;;
*)
  echo "[ERROR] RUN_MODE must be cpu|sim|npu (got ${RUN_MODE})" >&2
  exit 1
  ;;
esac

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

python3 "${CURRENT_DIR}/verify_result.py"
echo "[SUCCESS] pass-shake128-ops-math-toy (${RUN_MODE}, case=${SHAKE128_CASE})"
