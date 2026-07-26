#!/bin/bash
# pass-fix-f203-alg8-cbd-eta2-k4 — FIPS 203 Alg.8 SamplePolyCBD η=2（8×poly，k=4）
#
# 默认即最优路径：P2 双 AIV + DataCopy + SWAR+LUT（blockDim=2），无需配置编译开关。
#
# Usage：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#
# 环境变量（常规 run 可忽略）：
#   SEED_D — golden PRF 随机种子（默认 20260619）
#
# 测试专用 override（对照/回归时才设）：
#   CBD_TEST_BLOCK_DIM=1     — P1b-single（KeyGen prep 同构）
#   CBD_TEST_P0_SCALAR=ON    — P0 标量 CBD
#   CBD_TEST_P1A_SCALAR_IO=ON — P1a SWAR+LUT + scalar GM I/O
#
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

# 默认最优：P2 双 AIV
_CBD_BLOCK_DIM=2
_CBD_P0_SCALAR=OFF
_CBD_P1A_SCALAR_IO=OFF

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
export SEED_D="${SEED_D:-20260619}"

# 测试专用 override
if [ -n "${CBD_TEST_BLOCK_DIM:-}" ]; then
    _CBD_BLOCK_DIM="${CBD_TEST_BLOCK_DIM}"
fi
if [ "${CBD_TEST_P0_SCALAR:-}" = "ON" ]; then
    _CBD_P0_SCALAR=ON
fi
if [ "${CBD_TEST_P1A_SCALAR_IO:-}" = "ON" ]; then
    _CBD_P1A_SCALAR_IO=ON
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

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
fi

if [ -f "${_ASCEND_INSTALL_PATH}/set_env.sh" ]; then
    source "${_ASCEND_INSTALL_PATH}/set_env.sh"
elif [ -f "${_ASCEND_INSTALL_PATH}/bin/setenv.bash" ]; then
    source "${_ASCEND_INSTALL_PATH}/bin/setenv.bash"
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:$LD_LIBRARY_PATH
fi

echo "SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} SEED_D=${SEED_D} variant=P2(block_dim=${_CBD_BLOCK_DIM})"

set -e
rm -rf build out
mkdir -p build
cmake -B build \
    -DRUN_MODE=${RUN_MODE} \
    -DSOC_VERSION=${SOC_VERSION} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
    -DASCEND_CANN_PACKAGE_PATH=${_ASCEND_INSTALL_PATH} \
    -DF203_CBD_BLOCK_DIM=${_CBD_BLOCK_DIM} \
    -DCBD_P0_SCALAR=${_CBD_P0_SCALAR} \
    -DCBD_P1A_SCALAR_IO=${_CBD_P1A_SCALAR_IO}
cmake --build build -j
cmake --install build

rm -f ascendc_kernels_bbit
cp ./out/bin/ascendc_kernels_bbit ./
rm -rf input output
mkdir -p input output
python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH

export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-60}"
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

python3 "${CURRENT_DIR}/scripts/verify_result.py"
echo "[SUCCESS] pass-fix-f203-alg8-cbd-eta2-k4 (${RUN_MODE}) block_dim=${_CBD_BLOCK_DIM}"
