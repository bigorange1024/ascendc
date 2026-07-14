#!/usr/bin/env bash
# pass-fix-f203-alg19-kem-keygen-device-k4 — Alg.19 ML-KEM.KeyGen（2 launch · stable PKE + 内嵌 Alg.16 尾）
#
# 生产 I/O：
#   input/  — seed_d.bin + lut_even/odd_stacked.bin
#   output/ — ek_kem.bin (1568B) + dk_kem.bin (3168B)
#
# Usage（默认 = 生产全量 + golden 对拍，无需额外 env）：
#   bash run.sh -r cpu -v Ascend910B4
#   bash run.sh -r sim -v Ascend910B4
#
# PKE 源码：编译期引用 examples/stable/stable-fips203-mlkem-pke-keygen-k4（无 vendor/）
#
# P1 工程（2026-07-10 定案保留）：
#   - mmad：stable 宏 F203_KEM_KEYGEN_TAIL，不 fork mmad_custom_kem.cpp
#   - ROM：scripts/prep/gen_alg7_*.py → 本目录 prep/alg7/（PYTHONPATH 引 stable 的 alg7_geom）
#   - SIM tick ~713k（较 fork 首期 +~1.8%），I/O 与 correctness 一致；保留理由见 STATUS / INTEGRATION_PLAN §4.5
#
# 多环境分流（scripts/runtime_env.sh）：
#   bash run.sh -r auto -v Ascend910B4      # 单档最优 npu>sim>cpu（≠完整验收）
#   bash run.sh -r verify -v Ascend910B4    # cpu → SIM_DIRECT sim [→ npu，非WSL]
#   WSL 禁止 -r npu；说明见 docs/engineering/NPU真机环境说明.md

CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

_ORIG_ARGS=("$@")
if [ "${KEM_KEYGEN_KAT:-0}" = "1" ]; then
    mkdir -p "${CURRENT_DIR}/output"
    export CI=1
    exec >>"${CURRENT_DIR}/output/kat_liboqs_kem_keygen.log" 2>&1
fi

REPO_ROOT="$(cd "${CURRENT_DIR}/../.." && pwd)"
STABLE_KEYGEN="${REPO_ROOT}/examples/stable/stable-fips203-mlkem-pke-keygen-k4"
SCRIPTS_PREP="${CURRENT_DIR}/scripts/prep"
INSTALL_PREFIX=""
mkdir -p "${CURRENT_DIR}/scripts"
ln -sfn "${STABLE_KEYGEN}/scripts/compute" "${CURRENT_DIR}/scripts/compute"
ln -sfn "${STABLE_KEYGEN}/thirdparty" "${CURRENT_DIR}/thirdparty"

export SEED_D="${SEED_D:-20260619}"
export KEM_KEYGEN_VERIFY="${KEM_KEYGEN_VERIFY:-1}"
export KEM_KEYGEN_SKIP_REBUILD="${KEM_KEYGEN_SKIP_REBUILD:-${KEM_SKIP_REBUILD:-1}}"
export KEM_KEYGEN_FORCE_REBUILD="${KEM_KEYGEN_FORCE_REBUILD:-0}"
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-2}"
export KERNEL_COMPUTE_BUDGET_SEC="${KEM_KEYGEN_KERNEL_BUDGET_SEC:-900}"
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


# shellcheck source=/dev/null
source "${REPO_ROOT}/scripts/runtime_env.sh"
export ASCENDC_CASE_SUPPORTS_NPU="${ASCENDC_CASE_SUPPORTS_NPU:-1}"
runtime_env_dispatch "${BASH_SOURCE[0]}" "${_ORIG_ARGS[@]}"
if [ "${KEM_KG_EXT_SEED}" = "1" ]; then
    _DEFAULT_PROFILE="extseed"
else
    _DEFAULT_PROFILE="prod"
fi
BUILD_PROFILE="${KEM_KEYGEN_BUILD_PROFILE:-${_DEFAULT_PROFILE}}"
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

if [ "${RUN_MODE}" = "sim" ]; then
    export SIM_DIRECT=1
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib:${_ASCEND_INSTALL_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${_ASCEND_INSTALL_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
fi

if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
    echo "[kem_keygen device-k4] RUN_MODE=${RUN_MODE} SEED_D=${SEED_D} profile=${BUILD_PROFILE} BUDGET_SEC=${KERNEL_COMPUTE_BUDGET_SEC}"
fi

_keygen_gen_alg7_roms() {
    # ROM 写入本探针 prep/alg7/，避免污染 stable 树（与 stable 脚本逻辑同型，仅 out 路径不同）
    mkdir -p "${CURRENT_DIR}/prep/alg7"
    export PYTHONPATH="${STABLE_KEYGEN}/scripts/prep:${PYTHONPATH:-}"
    python3 "${SCRIPTS_PREP}/gen_alg7_interleave_rom.py"
    python3 "${SCRIPTS_PREP}/gen_alg7_deinterleave_rom.py"
    python3 "${SCRIPTS_PREP}/gen_alg7_compact_lut.py"
}

_build_stamp="${BUILD_DIR}/.kem_keygen_run_mode"
_build_tag="${BUILD_PROFILE}:${RUN_MODE}:extseed=${KEM_KG_EXT_SEED}"
_need_build=1
if [ "${KEM_KEYGEN_FORCE_REBUILD}" = "1" ]; then
    _need_build=1
elif [ "${KEM_KEYGEN_SKIP_REBUILD}" = "1" ] && [ -x "${INSTALL_PREFIX}/bin/ascendc_kem_keygen_bbit" ] && \
     [ -f "${INSTALL_PREFIX}/lib/libascendc_kernels_${RUN_MODE}.so" ] && \
     [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" = "${_build_tag}" ]; then
    _need_build=0
fi

_kem_build() {
    if [ "${KEM_KEYGEN_FORCE_REBUILD}" = "1" ] || \
       { [ -f "${_build_stamp}" ] && [ "$(cat "${_build_stamp}")" != "${_build_tag}" ]; }; then
        rm -rf "${BUILD_DIR}" "${INSTALL_PREFIX}"
    fi
    mkdir -p "${BUILD_DIR}"
    _keygen_gen_alg7_roms
    cmake -B "${BUILD_DIR}" \
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
    cmake --build "${BUILD_DIR}" -j"${CMAKE_BUILD_JOBS}"
    cmake --install "${BUILD_DIR}"
    echo "${_build_tag}" >"${_build_stamp}"
}

set -e
mkdir -p "${CURRENT_DIR}/input" "${CURRENT_DIR}/output"
python3 "${CURRENT_DIR}/scripts/prepare_production_input.py"
if [ "${_need_build}" = "1" ]; then
    if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
        echo "[run.sh] build profile=${BUILD_PROFILE} RUN_MODE=${RUN_MODE} dir=${BUILD_DIR##*/} jobs=${CMAKE_BUILD_JOBS}"
    fi
    _kem_build
else
    if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
        echo "[run.sh] skip rebuild (profile=${BUILD_PROFILE}, RUN_MODE=${RUN_MODE})"
    fi
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
        echo "[kem_keygen device-k4] output OK ek_kem=${ek_sz}B dk_kem=${dk_sz}B"
    fi
fi

if [ "${KEM_KEYGEN_KAT:-0}" != "1" ]; then
    echo "[SUCCESS] pass-fix-f203-alg19-kem-keygen-device-k4 (${RUN_MODE}) SEED_D=${SEED_D}"
fi
