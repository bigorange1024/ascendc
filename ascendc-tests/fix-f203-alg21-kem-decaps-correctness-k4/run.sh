#!/usr/bin/env bash
# fix-f203-alg21-kem-decaps-correctness-k4 — Alg.21 ML-KEM.Decaps 设备全链
#
# 生产 I/O：
#   input/  — dk_kem.bin（alg19）+ c.bin（alg20）+ LUT
#   output/ — K.bin (32B)
#
# Usage（默认 = 生产全量 + golden 对拍，无需额外 env）：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#
# 默认行为（2026-07-03）：
#   KEM_DECAPS_VERIFY=1         — 对拍 golden_K（alg20 K.bin）
#   KEM_DECAPS_SKIP_REBUILD=1   — 二进制与 RUN_MODE stamp 在则跳过 cmake
#   SIM 默认 ASCENDC_SIM_HOST_MODE=decaps_2session（2-session + 设备 FO）
#
# 环境（可选）：
#   KEM_DECAPS_FORCE_REBUILD=1  — 强制 rm -rf build out 后全量重编
#   CMAKE_BUILD_JOBS=2
#   DK_KEM_SRC / C_SRC / K_ENC_SRC
#
# 调试（非默认）：
#   KEM_DECAPS_VERIFY=0 bash run.sh -r cpu -v Ascend910B4
#   ASCENDC_SIM_HOST_MODE=decaps_1session bash run.sh -r sim  — 单 session 排障
#   KEM_DECAPS_TAMPER_C=1 bash run.sh -r sim  — 设备 FO 拒绝路径 J(z‖c)
#   （deprecated）KEM_DECAPS_SIM_2SESSION=0/1 仍映射到 ASCENDC_SIM_HOST_MODE
#
# Build profile 隔离（2026-07-03）：
#   默认生产/round-trip 用 profile=prod；liboqs kat quiet 路径用 profile=kat。
#   目录键 = build_<profile>_<run_mode> / out_<profile>_<run_mode>，可用
#   KEM_DECAPS_BUILD_PROFILE=<name> 显式覆盖。
#
# 多环境分流（scripts/runtime_env.sh）：
#   bash run.sh -r auto -v Ascend910B4      # 单档最优 npu>sim>cpu（≠完整验收）
#   bash run.sh -r verify -v Ascend910B4    # cpu → SIM_DIRECT sim [→ npu，非WSL]
#   WSL 禁止 -r npu；说明见 docs/engineering/NPU真机环境说明.md

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

_ORIG_ARGS=("$@")
if [ "${KEM_DECAPS_KAT:-0}" = "1" ]; then
    mkdir -p "${CURRENT_DIR}/output"
    export CI=1
    exec >>"${CURRENT_DIR}/output/kat_liboqs_kem_decaps.log" 2>&1
fi

REPO_ROOT="$(cd "${CURRENT_DIR}/../.." && pwd)"
INSTALL_PREFIX=""
export SEED_D="${SEED_D:-20260619}"
export DK_KEM_SRC="${DK_KEM_SRC:-${CURRENT_DIR}/../fix-f203-alg19-kem-keygen-correctness-k4/output/dk_kem.bin}"
export C_SRC="${C_SRC:-${CURRENT_DIR}/../fix-f203-alg20-kem-encaps-correctness-k4/output/c.bin}"
export K_ENC_SRC="${K_ENC_SRC:-${CURRENT_DIR}/../fix-f203-alg20-kem-encaps-correctness-k4/output/K.bin}"
export KEM_DECAPS_VERIFY="${KEM_DECAPS_VERIFY:-1}"
export KEM_DECAPS_TAMPER_C="${KEM_DECAPS_TAMPER_C:-0}"
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


# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/runtime_env.sh"
export ASCENDC_CASE_SUPPORTS_NPU="${ASCENDC_CASE_SUPPORTS_NPU:-1}"
runtime_env_dispatch "${BASH_SOURCE[0]}" "${_ORIG_ARGS[@]}"
if [ "${KEM_DECAPS_KAT:-0}" = "1" ]; then
    _DEFAULT_PROFILE="kat"
else
    _DEFAULT_PROFILE="prod"
fi
BUILD_PROFILE="${KEM_DECAPS_BUILD_PROFILE:-${_DEFAULT_PROFILE}}"
BUILD_DIR="${CURRENT_DIR}/build_${BUILD_PROFILE}_${RUN_MODE}"
if [ -z "${INSTALL_PREFIX}" ]; then
    INSTALL_PREFIX="${CURRENT_DIR}/out_${BUILD_PROFILE}_${RUN_MODE}"
fi

# SIM：默认 decaps_2session（见 library/shared/ascendc_build_mode.hpp）
if [ "${RUN_MODE}" = "sim" ]; then
    if [ -z "${ASCENDC_SIM_HOST_MODE:-}" ] && [ -n "${KEM_DECAPS_SIM_2SESSION:-}" ]; then
        if [ "${KEM_DECAPS_SIM_2SESSION}" = "0" ]; then
            export ASCENDC_SIM_HOST_MODE=decaps_1session
        else
            export ASCENDC_SIM_HOST_MODE=decaps_2session
        fi
    fi
    export ASCENDC_SIM_HOST_MODE="${ASCENDC_SIM_HOST_MODE:-decaps_2session}"
fi

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

_build_stamp="${BUILD_DIR}/.kem_decaps_run_mode"
_build_tag="${BUILD_PROFILE}:${RUN_MODE}"
_need_build=1
if [ "${KEM_DECAPS_FORCE_REBUILD}" = "1" ]; then
    _need_build=1
elif [ "${KEM_DECAPS_SKIP_REBUILD}" = "1" ] && [ -x "${INSTALL_PREFIX}/bin/ascendc_kem_decaps_bbit" ] && \
     [ -f "${INSTALL_PREFIX}/lib/libascendc_kernels_${RUN_MODE}.so" ] && \
     [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" = "${_build_tag}" ]; then
    _need_build=0
fi

_decaps_build() {
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_alg15_decrypt.sh"
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_alg14_encrypt.sh"
    if [ "${KEM_DECAPS_FORCE_REBUILD}" = "1" ] || \
       { [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" != "${_build_tag}" ]; }; then
        rm -rf "${BUILD_DIR}" "${INSTALL_PREFIX}"
    fi
    mkdir -p "${BUILD_DIR}"
    cmake -S "${CURRENT_DIR}/cmake/decaps" -B "${BUILD_DIR}" \
        -DRUN_MODE="${RUN_MODE}" \
        -DSOC_VERSION="${SOC_VERSION}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}"
    cmake --build "${BUILD_DIR}" -j"${CMAKE_BUILD_JOBS}"
    cmake --install "${BUILD_DIR}"
    echo "${_build_tag}" >"${_build_stamp}"
}

set -e
if [ "${_need_build}" = "1" ]; then
    if [ "${KEM_DECAPS_KAT:-0}" != "1" ]; then
        echo "[run.sh] build profile=${BUILD_PROFILE} RUN_MODE=${RUN_MODE} dir=${BUILD_DIR##*/} jobs=${CMAKE_BUILD_JOBS}"
    fi
    _decaps_build
else
    if [ "${KEM_DECAPS_KAT:-0}" != "1" ]; then
        echo "[run.sh] skip rebuild (profile=${BUILD_PROFILE}, RUN_MODE=${RUN_MODE})"
    fi
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_alg15_decrypt.sh"
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_alg14_encrypt.sh"
fi

rm -f "${CURRENT_DIR}/ascendc_kem_decaps_bbit"
cp -f "${INSTALL_PREFIX}/bin/ascendc_kem_decaps_bbit" "${CURRENT_DIR}/"
mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"

export SEED_D DK_KEM_SRC C_SRC K_ENC_SRC KEM_DECAPS_TAMPER_C
python3 "${CURRENT_DIR}/scripts/gen_data.py"

export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${INSTALL_PREFIX}/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"

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

if [ "${KEM_DECAPS_KAT:-0}" = "1" ]; then
    k_sz=$(wc -c <"${CURRENT_DIR}/output/K.bin")
    if [ "${k_sz}" -ne 32 ]; then
        echo "[ERROR] output K size=${k_sz}"
        exit 1
    fi
elif [ "${KEM_DECAPS_VERIFY}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/verify_kem_decaps.py"
fi

if [ "${KEM_DECAPS_KAT:-0}" != "1" ]; then
    echo "[SUCCESS] fix-f203-alg21-kem-decaps-correctness-k4 (${RUN_MODE}) SEED_D=${SEED_D}"
fi
