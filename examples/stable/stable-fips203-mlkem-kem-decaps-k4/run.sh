#!/usr/bin/env bash
# stable-fips203-mlkem-kem-decaps-k4 — FIPS 203 Alg.21/18 Decaps（vendored Decrypt+Encrypt + 设备 FO）
#
# customspec：stable-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex
# 生产 I/O：input/{dk_kem,c,lut_*} → output/K.bin
# PKE：本目录 pke_decrypt/ + prep/compute/ vendored；见 SELF_CONTAINED.md
#
# Usage（默认 = 全量；SIM 默认 decaps_2session）:
#   cd examples/stable/stable-fips203-mlkem-kem-decaps-k4
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#
# 调试（非默认）:
#   KEM_DECAPS_PHASEE_ONLY=1 …
#   KEM_DECAPS_REJECT=1 …
#   KEM_DECAPS_TAMPER_C=1 …
#   ASCENDC_SIM_HOST_MODE=decaps_1session …
#   KEM_DECAPS_FORCE_REBUILD=1 …

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
_ORIG_ARGS=("$@")
REPO_ROOT="$(cd "${CURRENT_DIR}/../../.." && pwd)"

# host_golden / thirdparty 已 vendored 于本目录（禁止软链其它 examples）

export KEM_DECAPS_VERIFY="${KEM_DECAPS_VERIFY:-1}"
export KEM_DECAPS_SKIP_REBUILD="${KEM_DECAPS_SKIP_REBUILD:-${KEM_SKIP_REBUILD:-1}}"
export KEM_DECAPS_FORCE_REBUILD="${KEM_DECAPS_FORCE_REBUILD:-0}"
export KEM_DECAPS_PHASEE_ONLY="${KEM_DECAPS_PHASEE_ONLY:-0}"
# TAMPER_C：与 correctness 笔记同义别名 → REJECT（假密文 / 拒绝路径）
if [ "${KEM_DECAPS_TAMPER_C:-0}" = "1" ]; then
    export KEM_DECAPS_REJECT=1
fi
export KEM_DECAPS_REJECT="${KEM_DECAPS_REJECT:-0}"
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-2}"
export KERNEL_COMPUTE_BUDGET_SEC="${KEM_DECAPS_KERNEL_BUDGET_SEC:-1800}"
# SIM 生产默认 2-session（教材第7章 CT / 用户锁定）；单 session 须显式覆盖
if [ -z "${ASCENDC_SIM_HOST_MODE:-}" ]; then
    export ASCENDC_SIM_HOST_MODE=decaps_2session
fi

BUILD_TYPE="Debug"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"
INSTALL_PREFIX=""

SHORT=r:,v:,i:,b:,p:
LONG=run-mode:,soc-version:,install-path:,build-type:,install-prefix:
OPTS=$(getopt -a --options "$SHORT" --longoptions "$LONG" -- "$@")
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

if [ "${KEM_DECAPS_PHASEE_ONLY}" = "1" ]; then
    BUILD_PROFILE="${KEM_DECAPS_BUILD_PROFILE:-phase_e}"
    BIN_NAME="ascendc_kem_decaps_phase_e_bbit"
    GEN_SCRIPT="gen_data_phase_e.py"
    LABEL="Phase-E-only"
elif [ "${KEM_DECAPS_REJECT}" = "1" ]; then
    BUILD_PROFILE="${KEM_DECAPS_BUILD_PROFILE:-prod}"
    BIN_NAME="ascendc_kem_decaps_bbit"
    GEN_SCRIPT="gen_data.py"
    LABEL="REJECT(E3)"
else
    BUILD_PROFILE="${KEM_DECAPS_BUILD_PROFILE:-prod}"
    BIN_NAME="ascendc_kem_decaps_bbit"
    GEN_SCRIPT="gen_data.py"
    LABEL="full D+E"
fi

BUILD_DIR="${CURRENT_DIR}/build_${BUILD_PROFILE}_${RUN_MODE}"
if [ -z "${INSTALL_PREFIX}" ]; then
    INSTALL_PREFIX="${CURRENT_DIR}/out_${BUILD_PROFILE}_${RUN_MODE}"
fi

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
elif [ -n "${ASCEND_INSTALL_PATH:-}" ] && [ -f "${ASCEND_INSTALL_PATH}/bin/setenv.bash" ]; then
    _ASCEND_INSTALL_PATH="${ASCEND_INSTALL_PATH}"
    # shellcheck source=/dev/null
    source "${_ASCEND_INSTALL_PATH}/bin/setenv.bash"
elif [ -d "$HOME/Ascend/cann" ]; then
    _ASCEND_INSTALL_PATH="$HOME/Ascend/cann"
else
    _ASCEND_INSTALL_PATH="/usr/local/Ascend/ascend-toolkit/latest"
fi
export ASCEND_TOOLKIT_HOME="${_ASCEND_INSTALL_PATH}"
export ASCEND_HOME_PATH="${_ASCEND_INSTALL_PATH}"
export CANN_HOME="${_ASCEND_INSTALL_PATH}"

set -euo pipefail

if [ "${RUN_MODE}" = "sim" ]; then
    export SIM_DIRECT="${SIM_DIRECT:-1}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
fi

echo "[kem_decaps ${LABEL}] RUN_MODE=${RUN_MODE} profile=${BUILD_PROFILE} SIM_HOST_MODE=${ASCENDC_SIM_HOST_MODE:-} BUDGET_SEC=${KERNEL_COMPUTE_BUDGET_SEC}"

_build_stamp="${BUILD_DIR}/.kem_decaps_run_mode"
_build_tag="${BUILD_PROFILE}:${RUN_MODE}"
_need_build=1
if [ "${KEM_DECAPS_FORCE_REBUILD}" = "1" ]; then
    _need_build=1
elif [ "${KEM_DECAPS_SKIP_REBUILD}" = "1" ] && [ -x "${INSTALL_PREFIX}/bin/${BIN_NAME}" ] && \
     [ -f "${INSTALL_PREFIX}/lib/libascendc_kernels_${RUN_MODE}.so" ] && \
     [ ! -f "${INSTALL_PREFIX}/lib/libascendc_kernels_dec_${RUN_MODE}.so" ] && \
     [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" = "${_build_tag}" ]; then
    _need_build=0
fi

_kem_build() {
    if [ "${KEM_DECAPS_FORCE_REBUILD}" = "1" ] || \
       { [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" != "${_build_tag}" ]; }; then
        rm -rf "${BUILD_DIR}" "${INSTALL_PREFIX}"
    fi
    # SIM/NPU：合库前生成 Decrypt 冲突头隔离 shim（幂等；源=本目录 pke_decrypt）
    if [ "${RUN_MODE}" = "sim" ] || [ "${RUN_MODE}" = "npu" ]; then
        bash "${CURRENT_DIR}/scripts/prepare_dec_shim.sh"
    fi
    mkdir -p "${BUILD_DIR}"
    cmake -B "${BUILD_DIR}" \
        -S "${CURRENT_DIR}/cmake/decaps" \
        -DRUN_MODE="${RUN_MODE}" \
        -DSOC_VERSION="${SOC_VERSION}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}"
    cmake --build "${BUILD_DIR}" -j "${CMAKE_BUILD_JOBS}"
    cmake --install "${BUILD_DIR}"
    echo "${_build_tag}" > "${_build_stamp}"
}

if [ "${_need_build}" = "1" ]; then
    _kem_build
else
    echo "[kem_decaps] skip rebuild (stamp=${_build_tag})"
fi

python3 "${CURRENT_DIR}/scripts/${GEN_SCRIPT}"

export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${INSTALL_PREFIX}/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
rm -f "${CURRENT_DIR}/${BIN_NAME}"
cp -f "${INSTALL_PREFIX}/bin/${BIN_NAME}" "${CURRENT_DIR}/"

if [ "${RUN_MODE}" = "sim" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi

bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "${CURRENT_DIR}/${BIN_NAME}"

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}" || true
fi

if [ "${KEM_DECAPS_VERIFY}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/verify_kem_decaps.py"
fi
echo "[SUCCESS] stable-fips203-mlkem-kem-decaps-k4 ${LABEL} (${RUN_MODE})"
