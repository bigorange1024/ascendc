#!/usr/bin/env bash
# fix-f203-alg21-kem-decaps-k4 — Alg.21 ML-KEM.Decaps 设备全链
#
# 生产 I/O：
#   input/  — dk_kem.bin（alg19）+ c.bin（alg20）+ LUT
#   output/ — K.bin (32B)
#
# Usage（默认）：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#
# 环境（可选）：
#   KEM_DECAPS_SKIP_REBUILD=1   — 已有二进制且 RUN_MODE 未变时跳过 cmake
#   KEM_DECAPS_FORCE_REBUILD=1  — 强制 rm -rf build out 后全量重编
#   CMAKE_BUILD_JOBS=2
#   DK_KEM_SRC / C_SRC / K_ENC_SRC
#
# 调试（非默认）：
#   KEM_DECAPS_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
#   KEM_DECAPS_SIM_2SESSION=1  — SIM 回退：两段 session（aclFinalize 后 fresh 重加密），
#                                 仅在单库单 session 出问题时对照用；默认走单库单 session D→G→E→FO。

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT="$(cd "${CURRENT_DIR}/../.." && pwd)"
INSTALL_PREFIX="${CURRENT_DIR}/out"
export SEED_D="${SEED_D:-20260619}"
export DK_KEM_SRC="${DK_KEM_SRC:-${CURRENT_DIR}/../fix-f203-alg19-kem-keygen-k4/output/dk_kem.bin}"
export C_SRC="${C_SRC:-${CURRENT_DIR}/../fix-f203-alg20-kem-encaps-k4/output/c.bin}"
export K_ENC_SRC="${K_ENC_SRC:-${CURRENT_DIR}/../fix-f203-alg20-kem-encaps-k4/output/K.bin}"
export KEM_DECAPS_VERIFY="${KEM_DECAPS_VERIFY:-0}"
export KEM_DECAPS_SKIP_REBUILD="${KEM_DECAPS_SKIP_REBUILD:-1}"
export KEM_DECAPS_FORCE_REBUILD="${KEM_DECAPS_FORCE_REBUILD:-0}"
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-2}"
export KERNEL_COMPUTE_BUDGET_SEC="${KEM_DECAPS_KERNEL_BUDGET_SEC:-1800}"

BUILD_TYPE="Debug"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"

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

if [ ! -f "${DK_KEM_SRC}" ]; then
    echo "[run.sh] ERROR: dk_kem not found: ${DK_KEM_SRC}" >&2
    echo "[run.sh] Run alg19 KeyGen once first." >&2
    exit 2
fi
if [ ! -f "${C_SRC}" ]; then
    echo "[run.sh] ERROR: c not found: ${C_SRC}" >&2
    echo "[run.sh] Run alg20 Encaps once first." >&2
    exit 3
fi

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
elif [ -n "${ASCEND_INSTALL_PATH:-}" ] && [ -f "${ASCEND_INSTALL_PATH}/bin/setenv.bash" ]; then
    _ASCEND_INSTALL_PATH="${ASCEND_INSTALL_PATH}"
    # shellcheck source=/dev/null
    source "${_ASCEND_INSTALL_PATH}/bin/setenv.bash"
elif [ -d "$HOME/Ascend/ascend-toolkit/latest" ]; then
    _ASCEND_INSTALL_PATH="$HOME/Ascend/ascend-toolkit/latest"
    # shellcheck source=/dev/null
    source "${_ASCEND_INSTALL_PATH}/bin/setenv.bash" 2>/dev/null || true
else
    _ASCEND_INSTALL_PATH="${ASCEND_INSTALL_PATH:-/usr/local/Ascend/ascend-toolkit/latest}"
fi

export ASCEND_TOOLKIT_HOME="${_ASCEND_INSTALL_PATH}"
export ASCEND_HOME_PATH="${_ASCEND_INSTALL_PATH}"
export CANN_HOME="${_ASCEND_INSTALL_PATH}"

if [ "${RUN_MODE}" = "sim" ]; then
    export SIM_DIRECT=1
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
fi

_build_stamp="${CURRENT_DIR}/build/.kem_decaps_run_mode"
_need_build=1
if [ "${KEM_DECAPS_FORCE_REBUILD}" = "1" ]; then
    _need_build=1
elif [ "${KEM_DECAPS_SKIP_REBUILD}" = "1" ] && [ -x "${CURRENT_DIR}/ascendc_kem_decaps_bbit" ] && \
     [ -f "${INSTALL_PREFIX}/lib/libascendc_kernels_${RUN_MODE}.so" ] && \
     [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" = "${RUN_MODE}" ]; then
    _need_build=0
fi

_decaps_build() {
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_alg15_decrypt.sh"
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_alg14_encrypt.sh"
    if [ "${KEM_DECAPS_FORCE_REBUILD}" = "1" ]; then
        rm -rf "${CURRENT_DIR}/build" "${INSTALL_PREFIX}"
    fi
    mkdir -p "${CURRENT_DIR}/build"
    cmake -S "${CURRENT_DIR}/cmake/decaps" -B build \
        -DRUN_MODE="${RUN_MODE}" \
        -DSOC_VERSION="${SOC_VERSION}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}"
    cmake --build build -j"${CMAKE_BUILD_JOBS}"
    cmake --install build
    echo "${RUN_MODE}" >"${_build_stamp}"
}

set -e
if [ "${_need_build}" = "1" ]; then
    echo "[run.sh] build RUN_MODE=${RUN_MODE} jobs=${CMAKE_BUILD_JOBS}"
    (cd "${CURRENT_DIR}" && _decaps_build)
else
    echo "[run.sh] skip rebuild (KEM_DECAPS_SKIP_REBUILD=1, RUN_MODE=${RUN_MODE})"
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_alg15_decrypt.sh"
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_alg14_encrypt.sh"
fi

rm -f "${CURRENT_DIR}/ascendc_kem_decaps_bbit"
cp -f "${INSTALL_PREFIX}/bin/ascendc_kem_decaps_bbit" "${CURRENT_DIR}/"
mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"

export SEED_D DK_KEM_SRC C_SRC K_ENC_SRC
python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH="${CURRENT_DIR}/out/lib:${CURRENT_DIR}/out/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"

if [ "${RUN_MODE}" = "sim" ]; then
    export SOC_VERSION
    export CANN_HOME="${_ASCEND_INSTALL_PATH}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi

echo "[run.sh] kernel RUN_MODE=${RUN_MODE} budget_sec=${KERNEL_COMPUTE_BUDGET_SEC}"
bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "${CURRENT_DIR}/ascendc_kem_decaps_bbit"

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

if [ "${KEM_DECAPS_VERIFY}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/verify_kem_decaps.py"
fi

echo "[SUCCESS] fix-f203-alg21-kem-decaps-k4 (${RUN_MODE}) SEED_D=${SEED_D}"
