#!/bin/bash
# @probe pass-fix-f203-alg13-device-keygen-k4
# @file run.sh
# @layer host
# @role 探针主编排：gen_data → 编译 prep+compute+keygen → 两次 kernel launch → 生产 output 对拍。 / Orchestrates full Alg.13 keygen probe.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends scripts/prep、scripts/compute、cmake/*、CANN setenv、repo scripts/kernel-run-timeout.sh。
# @verify bash run.sh -r cpu|sim -v Ascend910B4；bash kat_liboqs_vs_ascendc.sh；无需手动 SIM_DIRECT/HAT_*。


# pass-fix-f203-alg13-device-keygen-k4 — Alg.13 全链 KeyGen（prep 双 AIV 并行 Â）
#
# 生产 I/O：
#   input/  — seed_d.bin + lut_even/odd_stacked.bin
#   output/ — ek_pke.bin (1568B) + dk_pke.bin (1536B)
#
# 设备 Launch：2 次（prep 行 3–15 | compute+行21 融合）；中间 GM 不落盘。
#
# Usage（默认 = 全量生产路径；无需手动 export HAT_* / SIM_DIRECT 等）：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4          # run.sh 在 sim 模式内自动 export SIM_DIRECT=1
#   bash kat_liboqs_vs_ascendc.sh              # liboqs KAT（CPU×10 + SIM×1）
#
# 调试（须显式指定，非默认验收）：
#   KEYGEN_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
#   KEYGEN_DEBUG_DUMP=1 bash run.sh -r sim -v Ascend910B4
#
# 可选环境变量：
#   SEED_D — 默认 20260619
#   KEYGEN_KERNEL_BUDGET_SEC — 默认 900（全链 SIM 计算段 timeout）
#
# 多环境分流（scripts/runtime_env.sh）：
#   bash run.sh -r auto -v Ascend910B4      # 单档最优 npu>sim>cpu（≠完整验收）
#   bash run.sh -r verify -v Ascend910B4    # cpu → SIM_DIRECT sim [→ npu，非WSL]
#   WSL 禁止 -r npu；说明见 docs/engineering/NPU真机环境说明.md

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

_ORIG_ARGS=("$@")
if [ "${KEYGEN_KAT:-0}" = "1" ]; then
    mkdir -p "${CURRENT_DIR}/output"
    export CI=1
    exec >>"${CURRENT_DIR}/output/kat_liboqs_vs_ascendc.log" 2>&1
fi

REPO_ROOT="$(cd "${CURRENT_DIR}/../.." && pwd)"
SCRIPTS_PREP="${CURRENT_DIR}/scripts/prep"
INSTALL_PREFIX="${CURRENT_DIR}/out"

export SEED_D="${SEED_D:-20260619}"
export KERNEL_COMPUTE_BUDGET_SEC="${KEYGEN_KERNEL_BUDGET_SEC:-900}"

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


# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/runtime_env.sh"
export ASCENDC_CASE_SUPPORTS_NPU="${ASCENDC_CASE_SUPPORTS_NPU:-1}"
runtime_env_dispatch "${BASH_SOURCE[0]}" "${_ORIG_ARGS[@]}"
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

if [ "${KEYGEN_KAT:-0}" != "1" ]; then
    echo "[keygen] RUN_MODE=${RUN_MODE} SEED_D=${SEED_D} BUDGET_SEC=${KERNEL_COMPUTE_BUDGET_SEC} SIM_DIRECT=${SIM_DIRECT:-0}"
fi

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
        ! -name 'ek_pke.bin' ! -name 'dk_pke.bin' ! -name 'kat_liboqs_vs_ascendc.log' -delete 2>/dev/null || true
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

bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "${CURRENT_DIR}/ascendc_keygen_bbit"

if [ "${RUN_MODE}" = "sim" ]; then
    camodel_sim_collect_stray "${CURRENT_DIR}"
fi

_keygen_scrub_output

if [ "${KEYGEN_KAT:-0}" = "1" ]; then
    :
elif [ "${KEYGEN_VERIFY:-0}" = "1" ]; then
    KEYGEN_GOLDEN_ONLY=1 python3 "${CURRENT_DIR}/scripts/gen_data.py"
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

if [ "${KEYGEN_KAT:-0}" != "1" ]; then
    echo "[keygen] done"
fi
