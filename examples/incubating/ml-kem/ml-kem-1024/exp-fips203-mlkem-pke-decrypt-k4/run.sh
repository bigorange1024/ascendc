#!/usr/bin/env bash
# exp-fips203-mlkem-pke-decrypt-k4 — FIPS 203 Alg.15 PKE Decrypt（k=4）
#
# customspec：exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.tex
# 基线：pass-fix-f203-alg15-pke-decrypt-device-k4（一次性 vendor；本目录自包含）
#
# 生产 I/O：
#   input/  — dk_pke.bin (1536B) + c.bin (1568B) + LUT
#   output/ — m.bin (32B)
#
# Launch：**单 kernel** f203_decrypt_device_fused（GATE flag 4/8 分隔 NTT|INTT）。
#
# Usage（默认 = 全量生产路径）：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4          # 自动 SIM_DIRECT=1
#
# 默认行为（对齐 Encrypt stable）：
#   DECRYPT_SKIP_REBUILD=1   — 二进制与 RUN_MODE stamp 在则跳过 cmake
#   CMAKE_BUILD_JOBS=2       — 限并行，WSL 友好
#
# 调试（须显式指定，非默认）：
#   DECRYPT_FORCE_REBUILD=1 bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=0 bash run.sh -r sim -v Ascend910B4
#   COMPRESS_1_VEC=0 / DECOMPRESS_D_VEC=0 bash run.sh ...
#
# KAT / 外部 fixture（由 kat_liboqs_vs_ascendc.sh / roundtrip 设置）：
#   DECRYPT_KAT=1              — 静默日志；跳过 gen_data；跳过 verify（由 KAT 对拍 liboqs）
#   DECRYPT_SKIP_GEN_DATA=1    — 仅跳过 gen_data（input 已由 prepare 写好；仍做 verify）

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

if [ "${DECRYPT_KAT:-0}" = "1" ]; then
    mkdir -p "${CURRENT_DIR}/output"
    export CI=1
    exec >>"${CURRENT_DIR}/output/kat_liboqs_vs_ascendc.log" 2>&1
fi

_REPO_CAND="$(cd "${CURRENT_DIR}/../.." && pwd)"
if [ -d "${_REPO_CAND}/library/shared" ]; then
    REPO_ROOT="${_REPO_CAND}"
else
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
fi

BUILD_TYPE="Debug"
INSTALL_PREFIX="${CURRENT_DIR}/out"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"
# 默认不写死 SEED_D；gen_data 用 SHA3/SHAKE
if [ -n "${SEED_D:-}" ]; then
    export SEED_D
fi
export DECRYPT_GATE="${DECRYPT_GATE:-0}"
export DECRYPT_VERIFY="${DECRYPT_VERIFY:-1}"
export DECRYPT_SKIP_REBUILD="${DECRYPT_SKIP_REBUILD:-1}"
export DECRYPT_FORCE_REBUILD="${DECRYPT_FORCE_REBUILD:-0}"
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-2}"
export KERNEL_COMPUTE_BUDGET_SEC="${DECRYPT_KERNEL_BUDGET_SEC:-600}"

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
    *) echo "[ERROR] option $1"; exit 1 ;;
    esac
done

if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
    # shellcheck source=/dev/null
    source "${HOME}/ascendc/scripts/env.sh"
    _ASCEND_INSTALL_PATH="${CANN_HOME}"
elif [ -n "${ASCEND_INSTALL_PATH:-}" ] && [ -f "${ASCEND_INSTALL_PATH}/bin/setenv.bash" ]; then
    _ASCEND_INSTALL_PATH="${ASCEND_INSTALL_PATH}"
elif [ -d "$HOME/Ascend/ascend-toolkit/latest" ]; then
    _ASCEND_INSTALL_PATH="$HOME/Ascend/ascend-toolkit/latest"
else
    _ASCEND_INSTALL_PATH=/usr/local/Ascend/ascend-toolkit/latest
fi
export ASCEND_TOOLKIT_HOME="${_ASCEND_INSTALL_PATH}"
export ASCEND_HOME_PATH="${_ASCEND_INSTALL_PATH}"

if [ "${RUN_MODE}" = "sim" ]; then
    export SIM_DIRECT="${SIM_DIRECT:-1}"
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
fi

export COMPRESS_1_VEC="${COMPRESS_1_VEC:-1}"
export DECOMPRESS_D_VEC="${DECOMPRESS_D_VEC:-1}"

BUILD_DIR="${CURRENT_DIR}/build"
_build_stamp="${BUILD_DIR}/.decrypt_exp_run_mode"
_build_tag="${RUN_MODE}:c1=${COMPRESS_1_VEC}:d=${DECOMPRESS_D_VEC}"

_need_build=1
if [ "${DECRYPT_FORCE_REBUILD}" = "1" ]; then
    _need_build=1
elif [ "${DECRYPT_SKIP_REBUILD}" = "1" ] && [ -x "${INSTALL_PREFIX}/bin/ascendc_kernels_bbit" ] && \
     [ -f "${INSTALL_PREFIX}/lib/libascendc_kernels_${RUN_MODE}.so" ] && \
     [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" = "${_build_tag}" ]; then
    _need_build=0
fi

_decrypt_build() {
    if [ "${DECRYPT_FORCE_REBUILD}" = "1" ] || \
       { [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" != "${_build_tag}" ]; }; then
        rm -rf "${BUILD_DIR}" "${INSTALL_PREFIX}"
    fi
    mkdir -p "${BUILD_DIR}"
    cmake -B "${BUILD_DIR}" \
        -DRUN_MODE="${RUN_MODE}" \
        -DSOC_VERSION="${SOC_VERSION}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}" \
        -DCOMPRESS_1_VEC="${COMPRESS_1_VEC}" \
        -DDECOMPRESS_D_VEC="${DECOMPRESS_D_VEC}"
    cmake --build "${BUILD_DIR}" -j"${CMAKE_BUILD_JOBS}"
    cmake --install "${BUILD_DIR}"
    echo "${_build_tag}" >"${_build_stamp}"
}

set -e
if [ "${_need_build}" = "1" ]; then
    echo "[run.sh] build RUN_MODE=${RUN_MODE} jobs=${CMAKE_BUILD_JOBS}"
    _decrypt_build
else
    echo "[run.sh] skip rebuild (RUN_MODE=${RUN_MODE}, stamp OK)"
fi

rm -f "${CURRENT_DIR}/ascendc_kernels_bbit"
cp -f "${INSTALL_PREFIX}/bin/ascendc_kernels_bbit" "${CURRENT_DIR}/"
mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"
if [ "${DECRYPT_KAT:-0}" = "1" ] || [ "${DECRYPT_SKIP_GEN_DATA:-0}" = "1" ]; then
    echo "[run.sh] skip gen_data (external input)"
else
    rm -rf "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"
    mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"
    python3 "${CURRENT_DIR}/scripts/gen_data.py"
fi

export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${INSTALL_PREFIX}/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"

if [ "${RUN_MODE}" = "sim" ]; then
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-900}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
else
    export KERNEL_COMPUTE_BUDGET_SEC="${KERNEL_COMPUTE_BUDGET_SEC:-600}"
fi

/usr/bin/time -f '[wall_sec] %e' bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" ./ascendc_kernels_bbit 2>&1 | tee "${CURRENT_DIR}/output/run_metrics.txt"

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

if [ "${DECRYPT_GATE}" != "0" ]; then
    echo "[WARN] DECRYPT_GATE=${DECRYPT_GATE} 需要中间态 bin；生产路径已取消 mid D2H。" >&2
    DECRYPT_GATE="${DECRYPT_GATE}" python3 "${CURRENT_DIR}/scripts/verify_gate.py"
fi

if [ "${DECRYPT_KAT:-0}" = "1" ]; then
    if [ ! -f "${CURRENT_DIR}/output/m.bin" ] || [ "$(wc -c <"${CURRENT_DIR}/output/m.bin")" -ne 32 ]; then
        echo "[ERROR] DECRYPT_KAT: missing or bad-size output/m.bin"
        exit 1
    fi
elif [ "${DECRYPT_VERIFY}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/verify_result.py"
fi

echo "[SUCCESS] exp-fips203-mlkem-pke-decrypt-k4 (${RUN_MODE}) DECRYPT_VERIFY=${DECRYPT_VERIFY} COMPRESS_1_VEC=${COMPRESS_1_VEC} DECOMPRESS_D_VEC=${DECOMPRESS_D_VEC}"
