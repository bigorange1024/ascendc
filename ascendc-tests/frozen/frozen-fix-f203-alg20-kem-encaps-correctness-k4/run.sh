#!/usr/bin/env bash
# fix-f203-alg20-kem-encaps-correctness-k4 — Alg.20 ML-KEM.Encaps 设备全链
#
# 生产 I/O：
#   input/  — ek_kem.bin（复制自已生成的 pk，不触发 alg19）+ seed_d.bin + LUT
#   output/ — c.bin (1568B) + K.bin (32B)
#
# Usage（默认 = 生产全量 + golden 对拍，无需额外 env）：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#   bash run.sh -r npu -v Ascend910B4          # 仅真机；WSL 由 runtime_env 拒绝
#   RUN_WITH_MSPROF=1 MSPROF_MODE=app … -r npu  # 整进程 profiling + kernel_details；逐 launch 见 [npu_launch]
#
# 默认行为（2026-07-03）：
#   KEM_ENCAPS_VERIFY=1        — 对拍 golden_c/K
#   KEM_ENCAPS_SKIP_REBUILD=1  — 二进制与 RUN_MODE stamp 在则跳过 cmake
#
# 环境（可选）：
#   KEM_ENCAPS_FORCE_REBUILD=1  — 强制 rm -rf build out 后全量重编
#   CMAKE_BUILD_JOBS=2
#   EK_KEM_SRC=<path>           — pk 路径（默认 alg19/output/ek_kem.bin）
#
# 调试（非默认）：
#   KEM_ENCAPS_VERIFY=0 bash run.sh -r cpu -v Ascend910B4
#
# Build profile 隔离（2026-07-03）：
#   生产/round-trip（KEM_ENC_EXT_SEED=0）与 liboqs kat 旁路（=1）使用独立 build/install：
#   build_prod_cpu / out_prod_cpu、build_extseed_cpu / out_extseed_cpu 等。
#   可用 KEM_ENCAPS_BUILD_PROFILE=prod|extseed 显式覆盖。
#
# 多环境分流（scripts/runtime_env.sh）：
#   bash run.sh -r auto -v Ascend910B4      # 单档最优 npu>sim>cpu（≠完整验收）
#   bash run.sh -r verify -v Ascend910B4    # cpu → SIM_DIRECT sim [→ npu，非WSL]
#   WSL 禁止 -r npu；说明见 docs/engineering/NPU真机环境说明.md

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "${CURRENT_DIR}"

_ORIG_ARGS=("$@")
# KAT 批测 quiet：log → output/kat_liboqs_kem_encaps.log
if [ "${KEM_ENCAPS_KAT:-0}" = "1" ]; then
    mkdir -p "${CURRENT_DIR}/output"
    export CI=1
    exec >>"${CURRENT_DIR}/output/kat_liboqs_kem_encaps.log" 2>&1
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
INSTALL_PREFIX=""
export SEED_D="${SEED_D:-20260619}"
export KEM_ENCAPS_VERIFY="${KEM_ENCAPS_VERIFY:-1}"
export KEM_ENCAPS_SKIP_REBUILD="${KEM_ENCAPS_SKIP_REBUILD:-1}"
export KEM_ENCAPS_FORCE_REBUILD="${KEM_ENCAPS_FORCE_REBUILD:-0}"
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-2}"
export KERNEL_COMPUTE_BUDGET_SEC="${KEM_ENCAPS_KERNEL_BUDGET_SEC:-900}"
export KEM_ENC_EXT_SEED="${KEM_ENC_EXT_SEED:-0}"
# kat 批测可设 KEM_ENC_EK_SRC / EK_KEM_SRC 指向 output/kem_keypair_stash/ek_kem.bin
export EK_KEM_SRC="${EK_KEM_SRC:-${CURRENT_DIR}/../frozen-fix-f203-alg19-kem-keygen-correctness-k4/output/ek_kem.bin}"

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
if [ "${KEM_ENC_EXT_SEED}" = "1" ]; then
    _DEFAULT_PROFILE="extseed"
else
    _DEFAULT_PROFILE="prod"
fi
BUILD_PROFILE="${KEM_ENCAPS_BUILD_PROFILE:-${_DEFAULT_PROFILE}}"
BUILD_DIR="${CURRENT_DIR}/build_${BUILD_PROFILE}_${RUN_MODE}"
if [ -z "${INSTALL_PREFIX}" ]; then
    INSTALL_PREFIX="${CURRENT_DIR}/out_${BUILD_PROFILE}_${RUN_MODE}"
fi

if [ ! -f "${EK_KEM_SRC}" ]; then
    echo "[run.sh] ERROR: ek_kem not found: ${EK_KEM_SRC}" >&2
    echo "[run.sh] Run alg19 KeyGen once; do not embed KeyGen in this script." >&2
    exit 2
fi

# CANN + 分卡 + npu lib64：与 1024 stable run.sh 对齐（禁止 ${HOME}/ascendc 写死）
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/npu_case_env.sh"
npu_case_bootstrap || exit 1


_build_stamp="${BUILD_DIR}/.kem_encaps_run_mode"
_build_tag="${BUILD_PROFILE}:${RUN_MODE}:extseed=${KEM_ENC_EXT_SEED}"
_need_build=1
if [ "${KEM_ENCAPS_FORCE_REBUILD}" = "1" ]; then
    _need_build=1
elif [ "${KEM_ENCAPS_SKIP_REBUILD}" = "1" ] && [ -x "${INSTALL_PREFIX}/bin/ascendc_kem_encaps_bbit" ] && \
     [ -f "${INSTALL_PREFIX}/lib/libascendc_kernels_${RUN_MODE}.so" ] && \
     [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" = "${_build_tag}" ]; then
    _need_build=0
fi

_encaps_build() {
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_alg14_encrypt.sh"
    if [ "${KEM_ENCAPS_FORCE_REBUILD}" = "1" ] || \
       { [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" != "${_build_tag}" ]; }; then
        rm -rf "${BUILD_DIR}" "${INSTALL_PREFIX}"
    fi
    mkdir -p "${BUILD_DIR}"
    cmake -S "${CURRENT_DIR}/cmake/encaps" -B "${BUILD_DIR}" \
        -DRUN_MODE="${RUN_MODE}" \
        -DSOC_VERSION="${SOC_VERSION}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}" \
        -DKEM_ENC_EXT_SEED="${KEM_ENC_EXT_SEED}"
    cmake --build "${BUILD_DIR}" -j"${CMAKE_BUILD_JOBS}"
    cmake --install "${BUILD_DIR}"
    echo "${_build_tag}" >"${_build_stamp}"
}

set -e
if [ "${_need_build}" = "1" ]; then
    if [ "${KEM_ENCAPS_KAT:-0}" != "1" ]; then
        echo "[run.sh] build profile=${BUILD_PROFILE} RUN_MODE=${RUN_MODE} dir=${BUILD_DIR##*/} jobs=${CMAKE_BUILD_JOBS}"
    fi
    _encaps_build
else
    if [ "${KEM_ENCAPS_KAT:-0}" != "1" ]; then
        echo "[run.sh] skip rebuild (profile=${BUILD_PROFILE}, RUN_MODE=${RUN_MODE})"
    fi
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_alg14_encrypt.sh"
fi

rm -f "${CURRENT_DIR}/ascendc_kem_encaps_bbit"
cp -f "${INSTALL_PREFIX}/bin/ascendc_kem_encaps_bbit" "${CURRENT_DIR}/"
mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"

export SEED_D EK_KEM_SRC
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
# 默认直跑；RUN_WITH_MSPROF=1 时 npu 走 MSPROF_MODE=app 整进程采集
# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/msprof_run.sh"
msprof_run_kernel ./ascendc_kem_encaps_bbit

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
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
else
    c_sz=$(wc -c <"${CURRENT_DIR}/output/c.bin")
    k_sz=$(wc -c <"${CURRENT_DIR}/output/K.bin")
    if [ "${c_sz}" -ne 1568 ] || [ "${k_sz}" -ne 32 ]; then
        echo "[ERROR] output size c=${c_sz} K=${k_sz}"
        exit 1
    fi
    echo "[kem_encaps] output OK c=${c_sz}B K=${k_sz}B"
fi

if [ "${KEM_ENCAPS_KAT:-0}" != "1" ]; then
    echo "[SUCCESS] fix-f203-alg20-kem-encaps-correctness-k4 (${RUN_MODE}) SEED_D=${SEED_D}"
fi
