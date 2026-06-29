#!/bin/bash
# @exp exp-mlkem-f203-pke-keygen-k4
# @file run.sh
# @layer host
# @role 交付示例主编排：prepare input → build → 2 launch → 生产 output；SIM 打印各 launch tick 与加总。
# @production_io input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)。
# @depends scripts/prep、scripts/compute、cmake/keygen、library/shared、repo scripts/sim_env.sh。
# @verify bash run.sh -r cpu|sim -v Ascend910B4；KAT：bash kat_liboqs_vs_ascendc.sh

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# KAT 模式：整脚本日志重定向到 kat log，控制台由 kat 脚本打印精简进度
if [ "${KEYGEN_KAT:-0}" = "1" ]; then
    mkdir -p "${CURRENT_DIR}/output"
    export CI=1
    exec >>"${CURRENT_DIR}/output/kat_liboqs_vs_ascendc.log" 2>&1
fi

# exp-mlkem-f203-pke-keygen-k4 — 唯一向量化全链 KeyGen（seed+LUT → ek/dk）
#
# 唯一路径：main_keygen.cpp → 2 launch（prep | mmad+ek‖ρ）
# 无 G0–G4、无分段 CMake、无标量回退。
#
# Usage:
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4

REPO_ROOT="$(cd "${CURRENT_DIR}/../../.." && pwd)"
SCRIPTS_PREP="${CURRENT_DIR}/scripts/prep"
INSTALL_PREFIX="${CURRENT_DIR}/out"
KEYGEN_KERNEL_LOG="${CURRENT_DIR}/output/keygen_kernel.log"

export SEED_D="${SEED_D:-20260619}"
export KERNEL_COMPUTE_BUDGET_SEC="${KEYGEN_KERNEL_BUDGET_SEC:-1200}"

BUILD_TYPE="Debug"
SOC_VERSION="Ascend910B4"
RUN_MODE="cpu"

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

echo "[keygen] RUN_MODE=${RUN_MODE} SEED_D=${SEED_D} BUDGET_SEC=${KERNEL_COMPUTE_BUDGET_SEC} SIM_DIRECT=${SIM_DIRECT:-0}"

_keygen_gen_alg7_roms() {
    python3 "${SCRIPTS_PREP}/gen_alg7_interleave_rom.py"
    python3 "${SCRIPTS_PREP}/gen_alg7_deinterleave_rom.py"
    python3 "${SCRIPTS_PREP}/gen_alg7_compact_lut.py"
}

_keygen_build() {
    if [ "${KEYGEN_SKIP_REBUILD:-0}" = "1" ] && [ -x "${CURRENT_DIR}/ascendc_keygen_bbit" ]; then
        return 0
    fi
    rm -rf "${CURRENT_DIR}/build" "${INSTALL_PREFIX}"
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
        -DALG11_MEM_OPS=1
    cmake --build "${CURRENT_DIR}/build" -j"$(nproc)"
    cmake --install "${CURRENT_DIR}/build"
}

_keygen_prepare_input() {
    python3 "${CURRENT_DIR}/scripts/prepare_production_input.py"
}

_keygen_scrub_output() {
    mkdir -p "${CURRENT_DIR}/output"
    find "${CURRENT_DIR}/output" -mindepth 1 -maxdepth 1 -type f \
        ! -name 'ek_pke.bin' ! -name 'dk_pke.bin' ! -name 'kat_liboqs_vs_ascendc.log' \
        ! -name 'keygen_kernel.log' -delete 2>/dev/null || true
}

_keygen_print_sim_tick_summary() {
    [ "${RUN_MODE}" = "sim" ] || return 0
    python3 "${CURRENT_DIR}/scripts/parse_keygen_sim_metrics.py" "${CURRENT_DIR}" "${KEYGEN_KERNEL_LOG}"
}

_keygen_run_kernel() {
    local rc=0
    if [ "${RUN_MODE}" = "sim" ]; then
        mkdir -p "${CURRENT_DIR}/output"
        bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "${CURRENT_DIR}/ascendc_keygen_bbit" \
            2>&1 | tee "${KEYGEN_KERNEL_LOG}"
        rc=${PIPESTATUS[0]}
    else
        bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "${CURRENT_DIR}/ascendc_keygen_bbit"
        rc=$?
    fi
    return "${rc}"
}

set -e
mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"

_keygen_prepare_input
if [ "${KEYGEN_DEBUG_DUMP:-0}" = "1" ]; then mkdir -p "${CURRENT_DIR}/output/debug"; fi
_keygen_scrub_output
_keygen_build

export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${INSTALL_PREFIX}/lib64:${_ASCEND_INSTALL_PATH}/lib64:${LD_LIBRARY_PATH:-}"
rm -f "${CURRENT_DIR}/ascendc_keygen_bbit"
cp -f "${INSTALL_PREFIX}/bin/ascendc_keygen_bbit" "${CURRENT_DIR}/"

if [ "${RUN_MODE}" = "sim" ]; then
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/sim_env.sh"
    sim_env_export "${CURRENT_DIR}" "${REPO_ROOT}"
    # shellcheck source=/dev/null
    source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${CURRENT_DIR}"
fi

_keygen_run_kernel

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
    _keygen_print_sim_tick_summary
fi

_keygen_scrub_output

if [ "${KEYGEN_VERIFY:-0}" = "1" ]; then
    python3 "${CURRENT_DIR}/scripts/gen_data.py"
    python3 "${CURRENT_DIR}/scripts/verify_production.py"
else
    if [ ! -f "${CURRENT_DIR}/output/ek_pke.bin" ] || [ ! -f "${CURRENT_DIR}/output/dk_pke.bin" ]; then
        echo "[ERROR] missing output/ek_pke.bin or dk_pke.bin"
        exit 1
    fi
    ek_sz=$(wc -c <"${CURRENT_DIR}/output/ek_pke.bin")
    dk_sz=$(wc -c <"${CURRENT_DIR}/output/dk_pke.bin")
    if [ "${ek_sz}" -ne 1568 ] || [ "${dk_sz}" -ne 1536 ]; then
        echo "[ERROR] output size ek=${ek_sz} dk=${dk_sz}"
        exit 1
    fi
    echo "[keygen] output OK ek_pke=${ek_sz}B dk_pke=${dk_sz}B"
fi

echo "[keygen] done"
