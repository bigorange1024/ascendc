#!/bin/bash
# =============================================================================
# pass-fix-f203-alg13-lines8-15-se-k4 — 链式探针 8–17：预采样 (1×AIV) + NTT mixPass=5 (MIX)
# =============================================================================
#
# Usage：
#   bash run_chain_ntt17.sh -r cpu -v Ascend910B4
#   bash run_chain_ntt17.sh -r sim -v Ascend910B4
#
# 环境变量：
#   SEED_D — 默认 20260619
#   SE_VECTOR_STAGE — v3（默认）；v2.5 实验对照（更慢，不接入）
# =============================================================================
CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)
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
export SE_VECTOR_STAGE="${SE_VECTOR_STAGE:-v3}"

case "${RUN_MODE}" in
cpu | sim) ;;
*)
    echo "[ERROR] run_chain_ntt17.sh supports RUN_MODE=cpu|sim (got ${RUN_MODE})" >&2
    exit 1
    ;;
esac

case "${SE_VECTOR_STAGE}" in
v3) F203_SE_V25=OFF ;;
v2.5 | v25) F203_SE_V25=ON ;;
v4)
    echo "[WARN] SE_VECTOR_STAGE=v4 is obsolete; using v2.5" >&2
    F203_SE_V25=ON
    SE_VECTOR_STAGE=v2.5
    ;;
*)
    echo "[ERROR] SE_VECTOR_STAGE must be v3|v2.5" >&2
    exit 1
    ;;
esac

echo "CHAIN_NTT17 SOC=${SOC_VERSION} RUN_MODE=${RUN_MODE} SEED_D=${SEED_D} SE_VECTOR_STAGE=${SE_VECTOR_STAGE}"

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
fi

SIM_LIB="${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib"
if [ ! -d "${SIM_LIB}" ]; then
    SIM_LIB="${_ASCEND_INSTALL_PATH}/toolkit/tools/simulator/${SOC_VERSION}/lib"
fi

if [ "${RUN_MODE}" = "sim" ]; then
    export LD_LIBRARY_PATH="${CURRENT_DIR}/build/vec_k4_ntt/lib:${SIM_LIB}:${LD_LIBRARY_PATH:-}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH=${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${SIM_LIB}:${LD_LIBRARY_PATH:-}
fi

CHAIN_BIN="f203_se_chain_ntt17_${RUN_MODE}"
VEC_K4_BIN="${CURRENT_DIR}/build/vec_k4_ntt/ascendc_kernels_bbit"

set -e
rm -rf build input output
rm -f "${CURRENT_DIR}/f203_se_chain_ntt17_sim" "${CURRENT_DIR}/f203_se_chain_ntt17_cpu"
mkdir -p input output
python3 "${CURRENT_DIR}/gen_data_chain_ntt17.py"

cmake -S "${CURRENT_DIR}" -B build \
    -Dproduct_type="${SOC_VERSION}" \
    -Dsim_device="${SOC_VERSION}" \
    -Dtikicpu_lib_device="${SOC_VERSION}" \
    -Dccec_aicore_arch="${CCEC_ARCH}" \
    -Drun_mode="${RUN_MODE}" \
    -Df203_se_v25="${F203_SE_V25}" \
    -Df203_se_chain_ntt17=ON \
    -Dinstall_path="${_ASCEND_INSTALL_PATH}"
cmake --build build -j

if [ "${RUN_MODE}" = "sim" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-300}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi

if [ "${RUN_MODE}" = "cpu" ]; then
    "./${CHAIN_BIN}"
else
    echo "[chain-sim] Launch1 f203_se_vector_npu (SE 8–15)"
    bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "./f203_se_vector_npu"
    cp -f output/src.bin input/src.bin
    cp -f input/tiling_ntt.bin input/tiling.bin
    echo "[chain-sim] Launch2 vec-k4 mmad_custom mixPass=5 (NTT 16–17), src from Launch1"
    bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "${VEC_K4_BIN}"
fi

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

python3 "${CURRENT_DIR}/verify_chain_ntt17.py"
echo "[SUCCESS] chain ntt17 (Alg.13 lines 8–17) ${RUN_MODE} PASS"
