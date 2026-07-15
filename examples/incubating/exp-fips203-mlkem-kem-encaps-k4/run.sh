#!/usr/bin/env bash
# exp-fips203-mlkem-kem-encaps-k4 — FIPS 203 Alg.20/17 Encaps（vendored Encrypt + 设备 H/G）
#
# customspec：exp-fips203-mlkem-kem-encaps-k4-实现方案-customspec.tex
# 生产 I/O：input/{ek_kem,m,lut_*} → output/{c,K}.bin
#   $r$ 由设备 G 写出，Host 禁止预填 $r$ 为生产契约
# SIM：2 launch（f203_kem_enc_prep → f203_encrypt_l18_l19）
# CPU：5 launch（同 Encrypt 分叉；第 1 次为 kem prep）
#
# Usage（默认 = 全量；无需手动 export SIM_DIRECT）：
#   cd examples/incubating/exp-fips203-mlkem-kem-encaps-k4
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4          # 内置 SIM_DIRECT=1；WSL/Cloud 勿再手写
#   bash run.sh -r auto|-r verify …            # 见 runtime_env / NPU真机环境说明
#
# 调试（非默认）:
#   KEM_ENCAPS_FORCE_REBUILD=1 …
#   SIM_DIRECT=0 …（msprof）
#   M_HEX=… / M_FILE=… / EK_KEM_SRC=… / M_RANDOM=1
#   bash run.sh -r npu …                       # 仅真机；WSL 拒绝
#
# SIM：WSL dump 桩 / Cloud 不装桩 — scripts/sim_env.sh

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "${CURRENT_DIR}"
_ORIG_ARGS=("$@")

if [ "${KEM_ENCAPS_KAT:-0}" = "1" ]; then
    mkdir -p "${CURRENT_DIR}/output"
    export CI=1
    exec >>"${CURRENT_DIR}/output/kat_liboqs_kem_encaps.log" 2>&1
fi

# exp-* → incubating → examples → repo
REPO_ROOT="$(cd "${CURRENT_DIR}/../../.." && pwd)"

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
    # shellcheck source=/dev/null
    source "${_ASCEND_INSTALL_PATH}/bin/setenv.bash"
elif [ -d "$HOME/Ascend/ascend-toolkit/latest" ]; then
    _ASCEND_INSTALL_PATH="$HOME/Ascend/ascend-toolkit/latest"
    # shellcheck source=/dev/null
    source "${_ASCEND_INSTALL_PATH}/bin/setenv.bash"
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
elif [ "${RUN_MODE}" = "npu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
fi

echo "[kem_encaps exp] RUN_MODE=${RUN_MODE} profile=${BUILD_PROFILE} BUDGET_SEC=${KERNEL_COMPUTE_BUDGET_SEC} SIM_DIRECT=${SIM_DIRECT:-n/a}"

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

mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output" "${CURRENT_DIR}/golden"
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
    echo "[kem_encaps] SIM 2 launch；预算 ${KERNEL_COMPUTE_BUDGET_SEC}s SIM_DIRECT=${SIM_DIRECT}"
fi

bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kem_encaps_bbit

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}" || true
fi

if [ "${KEM_ENCAPS_KAT:-0}" = "1" ]; then
    c_sz=$(wc -c <"${CURRENT_DIR}/output/c.bin")
    k_sz=$(wc -c <"${CURRENT_DIR}/output/K.bin")
    if [ "${c_sz}" -ne 1568 ] || [ "${k_sz}" -ne 32 ]; then
        echo "[ERROR] output size c=${c_sz} K=${k_sz}"
        exit 1
    fi
elif [ "${KEM_ENCAPS_VERIFY}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/verify_kem_encaps.py"
fi
if [ "${KEM_ENCAPS_KAT:-0}" != "1" ]; then
    echo "[SUCCESS] exp-fips203-mlkem-kem-encaps-k4 (${RUN_MODE})"
fi
