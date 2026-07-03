#!/usr/bin/env bash
# fix-f203-alg19-kem-keygen-k4 — Alg.19 ML-KEM.KeyGen 设备全链
#
# 生产 I/O：
#   input/  — seed_d.bin + lut_even/odd_stacked.bin
#   output/ — ek_kem.bin (1568B) + dk_kem.bin (3168B)
#
# Usage（默认 = 生产全量 + golden 对拍，无需额外 env）：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#
# 默认行为（2026-07-03）：
#   KEM_KEYGEN_VERIFY=1      — 对拍 golden_ek/dk
#   KEM_KEYGEN_SKIP_REBUILD=1 — 二进制与 RUN_MODE stamp 在则跳过 cmake
#   生产 cmake 开关已在脚本/cmake 锁定为当前最优向量路径
#
# 调试（非默认）：
#   KEM_KEYGEN_VERIFY=0 bash run.sh -r cpu -v Ascend910B4   — 仅检查输出尺寸
#   KEM_KEYGEN_FORCE_REBUILD=1 bash run.sh -r cpu -v Ascend910B4

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# KAT 批测 quiet 路径：全量 log 落 output/kat_liboqs_kem_keygen.log，终端不刷屏（见 scripts/kat_liboqs_kem_keygen.py）
if [ "${KEM_KEYGEN_KAT:-0}" = "1" ]; then
    mkdir -p "${CURRENT_DIR}/output"
    export CI=1
    exec >>"${CURRENT_DIR}/output/kat_liboqs_kem_keygen.log" 2>&1
fi

REPO_ROOT="$(cd "${CURRENT_DIR}/../.." && pwd)"
INSTALL_PREFIX="${CURRENT_DIR}/out"
SCRIPTS_PREP="${CURRENT_DIR}/scripts/prep"

export SEED_D="${SEED_D:-20260619}"
export KEM_KEYGEN_VERIFY="${KEM_KEYGEN_VERIFY:-1}"
export KEM_KEYGEN_SKIP_REBUILD="${KEM_KEYGEN_SKIP_REBUILD:-${KEM_SKIP_REBUILD:-1}}"
export KEM_KEYGEN_FORCE_REBUILD="${KEM_KEYGEN_FORCE_REBUILD:-0}"
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-2}"
export KERNEL_COMPUTE_BUDGET_SEC="${KEM_KEYGEN_KERNEL_BUDGET_SEC:-900}"
# 旁路 A（test-only，默认关）：=1 时读 input/kem_seed.bin（d‖z，64B）喂 device，供 liboqs 交叉验证。
# 由 liboqs_kem_keygen_* 脚本置 1 并配 KEM_KEYGEN_VERIFY=0（内部 seed_d golden 不适用）。
export KEM_KG_EXT_SEED="${KEM_KG_EXT_SEED:-0}"

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

if [ "${RUN_MODE}" = "sim" ]; then
    export SIM_DIRECT=1
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
fi

if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
    echo "[kem_keygen] RUN_MODE=${RUN_MODE} SEED_D=${SEED_D} BUDGET_SEC=${KERNEL_COMPUTE_BUDGET_SEC}"
fi

_keygen_gen_alg7_roms() {
    python3 "${SCRIPTS_PREP}/gen_alg7_interleave_rom.py"
    python3 "${SCRIPTS_PREP}/gen_alg7_deinterleave_rom.py"
    python3 "${SCRIPTS_PREP}/gen_alg7_compact_lut.py"
}

_build_stamp="${CURRENT_DIR}/build/.kem_keygen_run_mode"
# stamp 含 KEM_KG_EXT_SEED：生产↔旁路 A 切换须强制重编（prep 入口与 z 来源不同）
_build_tag="${RUN_MODE}:extseed=${KEM_KG_EXT_SEED}"
_need_build=1
if [ "${KEM_KEYGEN_FORCE_REBUILD}" = "1" ]; then
    _need_build=1
elif [ "${KEM_KEYGEN_SKIP_REBUILD}" = "1" ] && [ -x "${CURRENT_DIR}/ascendc_kem_keygen_bbit" ] && \
     [ -f "${INSTALL_PREFIX}/lib/libascendc_kernels_${RUN_MODE}.so" ] && \
     [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" = "${_build_tag}" ]; then
    _need_build=0
fi

_kem_build() {
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_stable_keygen.sh"
    if [ "${KEM_KEYGEN_FORCE_REBUILD}" = "1" ]; then
        rm -rf "${CURRENT_DIR}/build" "${INSTALL_PREFIX}"
    fi
    mkdir -p "${CURRENT_DIR}/build"
    _keygen_gen_alg7_roms
    cmake -B "${CURRENT_DIR}/build" \
        -S "${CURRENT_DIR}/cmake/keygen" \
        -DRUN_MODE="${RUN_MODE}" \
        -DSOC_VERSION="${SOC_VERSION}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
        -DASCEND_CANN_PACKAGE_PATH="${_ASCEND_INSTALL_PATH}" \
        -DF203_AHAT16_BLOCK_DIM=2 \
        -DF203_ALG7_REJ_IMPL=1 \
        -DF203_ALG7_D12_GATHER=0 \
        -DF203_AHAT16_BATCH_SHAKE=0 \
        -DF203_ALG7_XOF_504=0 \
        -DF203_CBD_BLOCK_DIM=1 \
        -DF203_STAGE1_SPLIT=1 \
        -DHAT_LINE18_DOT_ONLY=0 \
        -DHAT_BYTE_ENCODE=1 \
        -DF203_PIPELINE_PROBE=0 \
        -DHAT_ALG11_VEC=1 \
        -DBYTE_ENCODE12_VEC=1 \
        -DBYTE_ENCODE12_SCATTER_VEC=1 \
        -DBYTE_ENCODE12_PREFETCH=1 \
        -DALG11_IMPL=1 \
        -DALG11_VEC_VARIANT=2 \
        -DALG11_VEC_OPTS=1 \
        -DALG11_MEM_OPS=1 \
        -DKEM_KG_EXT_SEED="${KEM_KG_EXT_SEED}"
    cmake --build "${CURRENT_DIR}/build" -j"${CMAKE_BUILD_JOBS}"
    cmake --install "${CURRENT_DIR}/build"
    echo "${_build_tag}" >"${_build_stamp}"
}

set -e
mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"
python3 "${CURRENT_DIR}/scripts/prepare_production_input.py"
if [ "${_need_build}" = "1" ]; then
    if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
        echo "[run.sh] build RUN_MODE=${RUN_MODE} jobs=${CMAKE_BUILD_JOBS}"
    fi
    _kem_build
else
    if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
        echo "[run.sh] skip rebuild (KEM_KEYGEN_SKIP_REBUILD=1, RUN_MODE=${RUN_MODE})"
    fi
    bash "${CURRENT_DIR}/scripts/vendor_sync_from_stable_keygen.sh"
fi

export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${INSTALL_PREFIX}/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
rm -f "${CURRENT_DIR}/ascendc_kem_keygen_bbit"
cp -f "${INSTALL_PREFIX}/bin/ascendc_kem_keygen_bbit" "${CURRENT_DIR}/"

if [ "${RUN_MODE}" = "sim" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi

bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "${CURRENT_DIR}/ascendc_kem_keygen_bbit"

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

if [ "${KEM_KEYGEN_KAT:-0}" = "1" ]; then
    ek_sz=$(wc -c <"${CURRENT_DIR}/output/ek_kem.bin")
    dk_sz=$(wc -c <"${CURRENT_DIR}/output/dk_kem.bin")
    if [ "${ek_sz}" -ne 1568 ] || [ "${dk_sz}" -ne 3168 ]; then
        echo "[ERROR] output size ek=${ek_sz} dk=${dk_sz}"
        exit 1
    fi
elif [ "${KEM_KEYGEN_VERIFY:-0}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/gen_data.py"
    python3 "${CURRENT_DIR}/scripts/verify_kem.py"
else
    ek_sz=$(wc -c <"${CURRENT_DIR}/output/ek_kem.bin")
    dk_sz=$(wc -c <"${CURRENT_DIR}/output/dk_kem.bin")
    if [ "${ek_sz}" -ne 1568 ] || [ "${dk_sz}" -ne 3168 ]; then
        echo "[ERROR] output size ek=${ek_sz} dk=${dk_sz}"
        exit 1
    fi
    if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
        echo "[kem_keygen] output OK ek_kem=${ek_sz}B dk_kem=${dk_sz}B"
    fi
fi

if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
    echo "[SUCCESS] fix-f203-alg19-kem-keygen-k4 (${RUN_MODE}) SEED_D=${SEED_D}"
fi
