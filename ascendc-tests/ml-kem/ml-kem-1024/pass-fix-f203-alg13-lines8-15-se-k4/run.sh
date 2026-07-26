#!/bin/bash
# =============================================================================
# pass-fix-f203-alg13-lines8-15-se-k4 — Alg.13 行 8–15：$s$/$e$ 预采样（SEED_D → src）
# =============================================================================
#
# Usage（默认 = V3 生产路径，无需 export SE_VECTOR_STAGE）：
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#
# 调试对照（须显式指定，非默认；更慢，禁止 KeyGen / vec-k4-v3 集成）：
#   SE_VECTOR_STAGE=v2.5 bash run.sh -r sim -v Ascend910B4
#
# 环境变量：
#   SEED_D — derand 种子（默认 20260619）
#   SE_VECTOR_STAGE — v3（默认）| v2.5（实验 bulk UB CBD）
#   VERIFY_STAGE — src（默认）| prf | all
#   F203_CBD_BLOCK_DIM — 默认 1（P1b-single，与 V3 锁定）
#
# 阶段宏（见 f203_se_stage_config.hpp）：
#   默认 CMake -DF203_SE_V25=OFF → F203_SE_VECTOR_V3=1
#   v2.5 → -DF203_SE_V25=ON → F203_SE_VECTOR_V25=1（历史名 v4 已废除）
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
REPO_ROOT="$(
  _d="${CURRENT_DIR}"
  while [ "${_d}" != "/" ]; do
    if [ -f "${_d}/AGENTS.md" ] && [ -d "${_d}/scripts" ]; then
      printf '%s\n' "${_d}"
      break
    fi
    _d="$(dirname "${_d}")"
  done
)"
if [ -z "${REPO_ROOT}" ] || [ ! -d "${REPO_ROOT}/scripts" ]; then
  echo "[ERROR] cannot locate repo root from ${CURRENT_DIR}" >&2
  exit 1
fi

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
# verify 编 C 参考需要 tiny_sha3（Cloud 若只 clone 了 ntt_onnx 会缺）
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
export SEED_D="${SEED_D:-20260619}"
export VERIFY_STAGE="${VERIFY_STAGE:-src}"
export F203_CBD_BLOCK_DIM="${F203_CBD_BLOCK_DIM:-1}"
export SE_VECTOR_STAGE="${SE_VECTOR_STAGE:-v3}"

case "${SE_VECTOR_STAGE}" in
v3)
    F203_SE_V25=OFF
    ;;
v2.5 | v25)
    F203_SE_V25=ON
    ;;
v4)
    echo "[WARN] SE_VECTOR_STAGE=v4 is obsolete; using v2.5 (experimental, not for integration)" >&2
    F203_SE_V25=ON
    SE_VECTOR_STAGE=v2.5
    ;;
*)
    echo "[ERROR] SE_VECTOR_STAGE must be v3|v2.5 (got ${SE_VECTOR_STAGE})" >&2
    exit 1
    ;;
esac

echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} SE_VECTOR_STAGE=${SE_VECTOR_STAGE} SEED_D=${SEED_D} VERIFY_STAGE=${VERIFY_STAGE} F203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM} F203_SE_V25=${F203_SE_V25}"

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
    -DF203_CBD_BLOCK_DIM=${F203_CBD_BLOCK_DIM} \
    -DF203_SE_V25=${F203_SE_V25}
cmake --build build -j
cmake --install build

rm -f ascendc_kernels_bbit f203_se_vector_cpu f203_se_vector_npu
cp ./out/bin/ascendc_kernels_bbit ./ascendc_kernels_bbit
if [ "${RUN_MODE}" = "cpu" ]; then
    cp ./ascendc_kernels_bbit ./f203_se_vector_cpu
elif [ "${RUN_MODE}" = "sim" ]; then
    cp ./ascendc_kernels_bbit ./f203_se_vector_npu
fi

rm -rf input output
mkdir -p input output
python3 "${CURRENT_DIR}/gen_data.py"

export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH

export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-180}"
if [ "${RUN_MODE}" = "sim" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi
bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit
if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

python3 "${CURRENT_DIR}/verify_result.py"
echo "[SUCCESS] pass-fix-f203-alg13-lines8-15-se-k4 stage=${SE_VECTOR_STAGE} (${RUN_MODE})"
