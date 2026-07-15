#!/usr/bin/env bash
# fix-f203-alg20-kem-encaps-device-k4 — Alg.20 Encaps（prep 前段 H/G + stable Encrypt）
#
# 生产 I/O：input/{ek_kem,m,lut_*} → output/{c,K}.bin；coins 设备自产，不读 Host coins 为权威。
# PKE：编译期引用 examples/stable/stable-fips203-mlkem-pke-encrypt-k4
#
# Usage（默认全量）:
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#
# 调试（非默认）:
#   KEM_ENCAPS_FORCE_REBUILD=1 …
#   SIM_DIRECT=0 …（msprof）

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
_ORIG_ARGS=("$@")
REPO_ROOT="$(cd "${CURRENT_DIR}/../.." && pwd)"
STABLE_ENCRYPT="${REPO_ROOT}/examples/stable/stable-fips203-mlkem-pke-encrypt-k4"

mkdir -p "${CURRENT_DIR}/scripts"
ln -sfn "${STABLE_ENCRYPT}/scripts/host_golden" "${CURRENT_DIR}/scripts/host_golden"
ln -sfn "${STABLE_ENCRYPT}/thirdparty" "${CURRENT_DIR}/thirdparty"

export KEM_ENCAPS_VERIFY="${KEM_ENCAPS_VERIFY:-1}"
export KEM_ENCAPS_SKIP_REBUILD="${KEM_ENCAPS_SKIP_REBUILD:-${KEM_SKIP_REBUILD:-1}}"
export KEM_ENCAPS_FORCE_REBUILD="${KEM_ENCAPS_FORCE_REBUILD:-0}"
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-2}"
export KERNEL_COMPUTE_BUDGET_SEC="${KEM_ENCAPS_KERNEL_BUDGET_SEC:-900}"

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

BUILD_PROFILE="${KEM_ENCAPS_BUILD_PROFILE:-prod}"
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

# 须在 source setenv 之后再 set -e：setenv 内 grep -q 失败会误杀本脚本
set -euo pipefail

if [ "${RUN_MODE}" = "sim" ]; then
    export SIM_DIRECT="${SIM_DIRECT:-1}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
fi

echo "[kem_encaps device-k4] RUN_MODE=${RUN_MODE} profile=${BUILD_PROFILE} BUDGET_SEC=${KERNEL_COMPUTE_BUDGET_SEC}"

_build_stamp="${BUILD_DIR}/.kem_encaps_run_mode"
_build_tag="${BUILD_PROFILE}:${RUN_MODE}"
_need_build=1
if [ "${KEM_ENCAPS_FORCE_REBUILD}" = "1" ]; then
    _need_build=1
elif [ "${KEM_ENCAPS_SKIP_REBUILD}" = "1" ] && [ -x "${INSTALL_PREFIX}/bin/ascendc_kem_encaps_bbit" ] && \
     [ -f "${INSTALL_PREFIX}/lib/libascendc_kernels_${RUN_MODE}.so" ] && \
     [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" = "${_build_tag}" ]; then
    _need_build=0
fi

_kem_build() {
    if [ "${KEM_ENCAPS_FORCE_REBUILD}" = "1" ] || \
       { [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" != "${_build_tag}" ]; }; then
        rm -rf "${BUILD_DIR}" "${INSTALL_PREFIX}"
    fi
    mkdir -p "${BUILD_DIR}"
    cmake -B "${BUILD_DIR}" \
        -S "${CURRENT_DIR}/cmake/encaps" \
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
    echo "[kem_encaps] skip rebuild (stamp=${_build_tag})"
fi

python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${INSTALL_PREFIX}/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
rm -f "${CURRENT_DIR}/ascendc_kem_encaps_bbit"
cp -f "${INSTALL_PREFIX}/bin/ascendc_kem_encaps_bbit" "${CURRENT_DIR}/"

if [ "${RUN_MODE}" = "sim" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi

bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "${CURRENT_DIR}/ascendc_kem_encaps_bbit"

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}" || true
fi

if [ "${KEM_ENCAPS_VERIFY}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/verify_kem_encaps.py"
fi
echo "[SUCCESS] fix-f203-alg20-kem-encaps-device-k4 (${RUN_MODE})"
