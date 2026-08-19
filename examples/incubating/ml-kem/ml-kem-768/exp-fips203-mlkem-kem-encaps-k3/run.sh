#!/usr/bin/env bash
# exp-fips203-mlkem-kem-encaps-k3 — Alg.20 Encaps（prep 前段 H/G + E14 k3 Encrypt 自包含副本）
#
# 生产 I/O：input/{ek_kem,m,lut_*} → output/{c,K}.bin；coins 设备自产，不读 Host coins 为权威。
# PKE：编译期使用本目录 vendored Encrypt/host_golden 副本；不依赖 ascendc-tests 或其它 exp。
#
# Usage（默认全量）:
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#   bash run.sh -r npu -v Ascend910B4          # 仅真机；WSL 由 runtime_env 拒绝
#   RUN_WITH_MSPROF=1 MSPROF_MODE=app … -r npu  # 整进程 profiling + kernel_details；逐 launch 见 [npu_launch]
#
# 调试（非默认）:
#   KEM_ENCAPS_FORCE_REBUILD=1 …
#   SIM_DIRECT=0 …（msprof）

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "${CURRENT_DIR}"
_ORIG_ARGS=("$@")

# 批测 quiet：log → output/kat_kem_encaps_k3.log
if [ "${KEM_ENCAPS_KAT:-0}" = "1" ]; then
    mkdir -p "${CURRENT_DIR}/output"
    export CI=1
    exec >>"${CURRENT_DIR}/output/kat_kem_encaps_k3.log" 2>&1
fi

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

# CANN + 分卡 + npu lib64：与 1024 stable run.sh 对齐（禁止 ${HOME}/ascendc 写死）
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/npu_case_env.sh"
npu_case_bootstrap || exit 1
set -euo pipefail


echo "[kem_encaps exp-k3] RUN_MODE=${RUN_MODE} profile=${BUILD_PROFILE} BUDGET_SEC=${KERNEL_COMPUTE_BUDGET_SEC}"

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

# 默认直跑；RUN_WITH_MSPROF=1 时 npu 走 MSPROF_MODE=app 整进程采集
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/msprof_run.sh"
msprof_run_kernel ./ascendc_kem_encaps_bbit

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}" || true
fi

if [ "${KEM_ENCAPS_KAT:-0}" = "1" ]; then
    c_sz=$(wc -c <"${CURRENT_DIR}/output/c.bin")
    k_sz=$(wc -c <"${CURRENT_DIR}/output/K.bin")
    if [ "${c_sz}" -ne 1088 ] || [ "${k_sz}" -ne 32 ]; then
        echo "[ERROR] output size c=${c_sz} K=${k_sz}"
        exit 1
    fi
elif [ "${KEM_ENCAPS_VERIFY}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/verify_kem_encaps.py"
fi
if [ "${KEM_ENCAPS_KAT:-0}" != "1" ]; then
    echo "[SUCCESS] exp-fips203-mlkem-kem-encaps-k3 (${RUN_MODE})"
fi
