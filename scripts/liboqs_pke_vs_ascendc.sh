#!/usr/bin/env bash
# liboqs_pke_vs_ascendc.sh — liboqs PKE 向量 ↔ AscendC KeyGen/Encrypt/Decrypt 三阶段对拍
#
# 流程：
#   1. liboqs 生成 fixture（seed_d, d, m, coins, ek, dk, c, m_rec）
#   2. KeyGen 探针（SEED_D）→ ek/dk 与 liboqs 比
#   3. Encrypt 探针（AscendC ek + liboqs m/coins）→ c 与 liboqs 比
#   4. Decrypt 探针（AscendC dk + AscendC c）→ m 与 liboqs m / m_rec 比
#
# Usage:
#   bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4
#   bash scripts/liboqs_pke_vs_ascendc.sh -r sim -v Ascend910B4
#
# 环境：
#   SEED_D              默认 20260619
#   LIBOQS_FIXTURE_DIR  默认 output/liboqs_pke_fixture/<SEED_D>/
#   LIBOQS_VS_SKIP_BUILD=1  复用已编译二进制

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KEYGEN_DIR="${KEYGEN_DIR:-${REPO_ROOT}/ascendc-tests/pass-fix-f203-alg13-device-keygen-k4}"
ENCRYPT_DIR="${ENCRYPT_DIR:-${REPO_ROOT}/examples/stable/stable-fips203-mlkem-pke-encrypt-k4}"
# Decrypt 默认 stable 定型；回退：DECRYPT_DIR=.../pass-fix-f203-alg15-pke-decrypt-device-k4
DECRYPT_DIR="${DECRYPT_DIR:-${REPO_ROOT}/examples/stable/stable-fips203-mlkem-pke-decrypt-k4}"

RUN_MODE="cpu"
SOC_VERSION="Ascend910B4"
export SEED_D="${SEED_D:-20260619}"
FIXTURE_DIR="${LIBOQS_FIXTURE_DIR:-${REPO_ROOT}/output/liboqs_pke_fixture/${SEED_D}}"
export ENCRYPT_GATE=5
# device-k4 生产无 mid D2H；L2 只验 m vs liboqs
export DECRYPT_GATE="${DECRYPT_GATE:-0}"
export ENCRYPT_VERIFY=0
export DECRYPT_VERIFY=0
export KEYGEN_VERIFY=0
export KERNEL_COMPUTE_BUDGET_SEC="${LIBOQS_VS_KERNEL_BUDGET_SEC:-900}"

SHORT=r:,v:
LONG=run-mode:,soc-version:
OPTS=$(getopt -a --options "$SHORT" --longoptions "$LONG" -- "$@")
eval set -- "$OPTS"
while :; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    --) shift; break ;;
    *) echo "[liboqs_vs_asc] unknown option $1" >&2; exit 1 ;;
    esac
done

_ascend_home() {
    if [ -f "${HOME}/ascendc/scripts/env.sh" ]; then
        # shellcheck source=/dev/null
        source "${HOME}/ascendc/scripts/env.sh"
        echo "${CANN_HOME}"
    elif [ -d "${HOME}/Ascend/ascend-toolkit/latest" ]; then
        echo "${HOME}/Ascend/ascend-toolkit/latest"
    else
        echo "/usr/local/Ascend/ascend-toolkit/latest"
    fi
}

ASCEND_HOME_PATH="$(_ascend_home)"
export ASCEND_TOOLKIT_HOME="${ASCEND_HOME_PATH}"
export ASCEND_HOME_PATH="${ASCEND_HOME_PATH}"

if [ "${RUN_MODE}" = "sim" ]; then
    export SIM_DIRECT=1
    export LD_LIBRARY_PATH="${ASCEND_HOME_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
elif [ "${RUN_MODE}" = "cpu" ]; then
    export LD_LIBRARY_PATH="${ASCEND_HOME_PATH}/tools/tikicpulib/lib:${ASCEND_HOME_PATH}/tools/tikicpulib/lib/${SOC_VERSION}:${ASCEND_HOME_PATH}/tools/simulator/${SOC_VERSION}/lib:${LD_LIBRARY_PATH:-}"
fi

echo "[liboqs_vs_asc] SEED_D=${SEED_D} RUN_MODE=${RUN_MODE} fixture=${FIXTURE_DIR}"

# --- Phase 0: liboqs fixture ---
python3 "${REPO_ROOT}/scripts/liboqs_pke_fixture.py" --seed-d "${SEED_D}" --out-dir "${FIXTURE_DIR}"

_prepare_keygen_input() {
    export SEED_D="${SEED_D}"
    python3 "${KEYGEN_DIR}/scripts/prepare_production_input.py"
}

_prepare_encrypt_input() {
    local inp="${ENCRYPT_DIR}/input"
    local out="${ENCRYPT_DIR}/output"
    mkdir -p "${inp}" "${out}"
    cp -f "${KEYGEN_DIR}/output/ek_pke.bin" "${inp}/ek_pke.bin"
    cp -f "${FIXTURE_DIR}/m.bin" "${inp}/m.bin"
    cp -f "${FIXTURE_DIR}/coins.bin" "${inp}/coins.bin"
    python3 - <<PY
import struct
from pathlib import Path
inp = Path("${inp}")
meta = struct.pack("<IIII", ${SEED_D}, 1568, 32, 1568)
inp.joinpath("meta.bin").write_bytes(meta)
PY
    python3 "${ENCRYPT_DIR}/scripts/host_golden/ntt_lut_bins.py" "${inp}"
    echo "[liboqs_vs_asc] encrypt input: AscendC ek + liboqs m/coins"
}

_prepare_decrypt_input() {
    local inp="${DECRYPT_DIR}/input"
    local out="${DECRYPT_DIR}/output"
    mkdir -p "${inp}" "${out}"
    cp -f "${KEYGEN_DIR}/output/dk_pke.bin" "${inp}/dk_pke.bin"
    cp -f "${ENCRYPT_DIR}/output/c.bin" "${inp}/c.bin"
    python3 - <<PY
import struct
from pathlib import Path
inp = Path("${inp}")
meta = struct.pack("<IIII", ${SEED_D}, 1536, 1568, 32)
inp.joinpath("meta.bin").write_bytes(meta)
PY
    python3 "${DECRYPT_DIR}/scripts/host_golden/ntt_lut_bins.py" "${inp}"
    echo "[liboqs_vs_asc] decrypt input: AscendC dk + AscendC c"
}

_build_keygen() {
    local stamp="${KEYGEN_DIR}/.liboqs_vs_built_mode"
    if [ "${LIBOQS_VS_FORCE_REBUILD:-0}" = "1" ]; then
        rm -f "${KEYGEN_DIR}/ascendc_keygen_bbit" "${stamp}"
    fi
    if [ -x "${KEYGEN_DIR}/ascendc_keygen_bbit" ] && [ -d "${KEYGEN_DIR}/out" ] && [ -f "${stamp}" ] \
        && [ "$(cat "${stamp}")" = "${RUN_MODE}" ]; then
        echo "[liboqs_vs_asc] reuse keygen binary (${RUN_MODE})"
        return 0
    fi
    if [ "${LIBOQS_VS_SKIP_BUILD:-0}" = "1" ]; then
        echo "[liboqs_vs_asc] ERROR: missing keygen binary" >&2
        exit 2
    fi
    echo "[liboqs_vs_asc] cmake build keygen (${RUN_MODE}) …"
    (
        cd "${KEYGEN_DIR}"
        python3 scripts/prep/gen_alg7_interleave_rom.py
        python3 scripts/prep/gen_alg7_deinterleave_rom.py
        python3 scripts/prep/gen_alg7_compact_lut.py
        rm -rf build out
        mkdir -p build
        cmake -B build \
            -S cmake/keygen \
            -DRUN_MODE="${RUN_MODE}" \
            -DSOC_VERSION="${SOC_VERSION}" \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_INSTALL_PREFIX="$(pwd)/out" \
            -DASCEND_CANN_PACKAGE_PATH="${ASCEND_HOME_PATH}" \
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
        cmake --build build -j"$(nproc)"
        cmake --install build
        cp -f out/bin/ascendc_keygen_bbit ./ascendc_keygen_bbit
        echo "${RUN_MODE}" > "${stamp}"
    )
}

_build_encrypt_decrypt() {
    local case_dir="$1"
    local bin_name="$2"
    local stamp="${case_dir}/.liboqs_vs_built_mode"
    if [ "${LIBOQS_VS_FORCE_REBUILD:-0}" = "1" ]; then
        rm -f "${case_dir}/${bin_name}" "${stamp}"
    fi
    if [ -x "${case_dir}/${bin_name}" ] && [ -d "${case_dir}/out" ] && [ -f "${stamp}" ] \
        && [ "$(cat "${stamp}")" = "${RUN_MODE}" ]; then
        echo "[liboqs_vs_asc] reuse ${case_dir}/${bin_name} (${RUN_MODE})"
        return 0
    fi
    if [ "${LIBOQS_VS_SKIP_BUILD:-0}" = "1" ]; then
        echo "[liboqs_vs_asc] ERROR: missing ${case_dir}/${bin_name}" >&2
        exit 2
    fi
    echo "[liboqs_vs_asc] cmake build ${case_dir} (${RUN_MODE}) …"
    (
        cd "${case_dir}"
        bash scripts/vendor_sync.sh
        rm -rf build out
        mkdir -p build
        cmake -B build \
            -DRUN_MODE="${RUN_MODE}" \
            -DSOC_VERSION="${SOC_VERSION}" \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_INSTALL_PREFIX="$(pwd)/out" \
            -DASCEND_CANN_PACKAGE_PATH="${ASCEND_HOME_PATH}"
        cmake --build build -j
        cmake --install build
        cp "./out/bin/${bin_name}" "./${bin_name}"
        echo "${RUN_MODE}" > "${stamp}"
    )
}

_run_kernel() {
    local case_dir="$1"
    local bin_name="$2"
    cd "${case_dir}"
    export LD_LIBRARY_PATH="$(pwd)/out/lib:$(pwd)/out/lib64:${ASCEND_HOME_PATH}/lib64:${LD_LIBRARY_PATH:-}"
    if [ "${RUN_MODE}" = "sim" ]; then
        # shellcheck source=/dev/null
        source "${REPO_ROOT}/scripts/sim_env.sh"
        sim_env_export "${case_dir}" "${REPO_ROOT}"
        # shellcheck source=/dev/null
        source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${case_dir}"
    fi
    mkdir -p output
    /usr/bin/time -f '[liboqs_vs_asc wall_sec] %e' \
        bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "./${bin_name}" 2>&1 | tee "${case_dir}/output/liboqs_vs_run_metrics.txt"
    if [ "${RUN_MODE}" = "sim" ]; then
        camodel_sim_collect_stray "${case_dir}"
    fi
}

_run_keygen_only() {
    _prepare_keygen_input
    _build_keygen
    cd "${KEYGEN_DIR}"
    export LD_LIBRARY_PATH="${KEYGEN_DIR}/out/lib:${KEYGEN_DIR}/out/lib64:${ASCEND_HOME_PATH}/lib64:${LD_LIBRARY_PATH:-}"
    if [ "${RUN_MODE}" = "sim" ]; then
        # shellcheck source=/dev/null
        source "${REPO_ROOT}/scripts/sim_env.sh"
        sim_env_export "${KEYGEN_DIR}" "${REPO_ROOT}"
        # shellcheck source=/dev/null
        source "${REPO_ROOT}/scripts/camodel_sim_log.sh" "${KEYGEN_DIR}"
    fi
    mkdir -p output
    /usr/bin/time -f '[liboqs_vs_asc wall_sec] %e' \
        bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "./ascendc_keygen_bbit" 2>&1 | tee "${KEYGEN_DIR}/output/liboqs_vs_run_metrics.txt"
    if [ "${RUN_MODE}" = "sim" ]; then
        camodel_sim_collect_stray "${KEYGEN_DIR}"
    fi
}

# --- Phase 1: KeyGen ---
echo "[liboqs_vs_asc] === Phase 1: device KeyGen vs liboqs ek/dk ==="
_run_keygen_only
python3 "${REPO_ROOT}/scripts/liboqs_pke_vs_ascendc_verify.py" \
    --stage keygen --fixture "${FIXTURE_DIR}" --ascendc "${KEYGEN_DIR}/output"

# --- Phase 2: Encrypt ---
echo "[liboqs_vs_asc] === Phase 2: device Encrypt vs liboqs c ==="
_build_encrypt_decrypt "${ENCRYPT_DIR}" "ascendc_kernels_bbit"
_prepare_encrypt_input
_run_kernel "${ENCRYPT_DIR}" "ascendc_kernels_bbit"
python3 "${REPO_ROOT}/scripts/liboqs_pke_vs_ascendc_verify.py" \
    --stage encrypt --fixture "${FIXTURE_DIR}" --ascendc "${ENCRYPT_DIR}/output"

# --- Phase 3: Decrypt ---
echo "[liboqs_vs_asc] === Phase 3: device Decrypt vs liboqs m / m_rec ==="
_build_encrypt_decrypt "${DECRYPT_DIR}" "ascendc_kernels_bbit"
_prepare_decrypt_input
_run_kernel "${DECRYPT_DIR}" "ascendc_kernels_bbit"
python3 "${REPO_ROOT}/scripts/liboqs_pke_vs_ascendc_verify.py" \
    --stage decrypt \
    --fixture "${FIXTURE_DIR}" \
    --ascendc "${DECRYPT_DIR}/output" \
    --ascendc-extra "${ENCRYPT_DIR}/input/m.bin"

echo "[SUCCESS] liboqs PKE vs AscendC KeyGen+Encrypt+Decrypt (${RUN_MODE}) SEED_D=${SEED_D}"
