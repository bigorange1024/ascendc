#!/usr/bin/env bash
# roundtrip_pke_encrypt_decrypt.sh — device KeyGen 密钥 → Encrypt(c) → Decrypt(m) 闭环
#
# Encrypt 默认：examples/incubating/exp-mlkem-f203-pke-encrypt-k4（自包含 run.sh）
# Decrypt 默认：ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4
#
# 前提：KeyGen 探针已产出 output/ek_pke.bin + dk_pke.bin（本脚本不重复跑 KeyGen，除非
#       ROUNDTRIP_BOOTSTRAP_KEYGEN=1 且文件缺失时一次性 bootstrap）。
#
# Usage（单次）:
#   bash scripts/roundtrip_pke_encrypt_decrypt.sh -r cpu -v Ascend910B4
#   bash scripts/roundtrip_pke_encrypt_decrypt.sh -r sim -v Ascend910B4
#
# 批跑（默认 CPU×10 + SIM×1，随机 SEED_D；见 roundtrip_pke_batch.sh）:
#   bash scripts/roundtrip_pke_batch.sh
#
# 环境（可选）：
#   KEYGEN_DIR / ENCRYPT_DIR / DECRYPT_DIR
#   SEED_D              默认 20260619（须与 m/coins 派生一致）
#   ROUNDTRIP_SKIP_BUILD=1   跳过 decrypt cmake（要求已有二进制）；encrypt 走 run.sh SKIP
#   ROUNDTRIP_BOOTSTRAP_KEYGEN=1  缺密钥时自动跑 KeyGen

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KEYGEN_DIR="${KEYGEN_DIR:-${REPO_ROOT}/ascendc-tests/pass-fix-f203-alg13-device-keygen-k4}"
ENCRYPT_DIR="${ENCRYPT_DIR:-${REPO_ROOT}/examples/stable/stable-mlkem-f203-pke-encrypt-k4}"
DECRYPT_DIR="${DECRYPT_DIR:-${REPO_ROOT}/ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4}"

RUN_MODE="cpu"
SOC_VERSION="Ascend910B4"
export SEED_D="${SEED_D:-20260619}"
export ENCRYPT_GATE=5
export DECRYPT_GATE=4
export ENCRYPT_VERIFY=0
export DECRYPT_VERIFY=0
export KERNEL_COMPUTE_BUDGET_SEC="${ROUNDTRIP_KERNEL_BUDGET_SEC:-900}"

SHORT=r:,v:
LONG=run-mode:,soc-version:
OPTS=$(getopt -a --options "$SHORT" --longoptions "$LONG" -- "$@")
eval set -- "$OPTS"
while :; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    --) shift; break ;;
    *) echo "[roundtrip] unknown option $1" >&2; exit 1 ;;
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

KEYGEN_OUT="${KEYGEN_DIR}/output"
EK_FILE="${KEYGEN_OUT}/ek_pke.bin"
DK_FILE="${KEYGEN_OUT}/dk_pke.bin"

if [ ! -f "${EK_FILE}" ] || [ ! -f "${DK_FILE}" ]; then
    if [ "${ROUNDTRIP_BOOTSTRAP_KEYGEN:-0}" = "1" ]; then
        echo "[roundtrip] bootstrap: KeyGen 缺密钥，一次性运行 KeyGen (${RUN_MODE}) …" >&2
        (cd "${KEYGEN_DIR}" && SEED_D="${SEED_D}" bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}")
    else
        echo "[roundtrip] ERROR: 缺少 device 密钥对：" >&2
        echo "  ${EK_FILE}" >&2
        echo "  ${DK_FILE}" >&2
        echo "请先在 KeyGen 探针跑通一次，或设 ROUNDTRIP_BOOTSTRAP_KEYGEN=1" >&2
        exit 2
    fi
fi

_is_exp_encrypt() {
    [ -f "${ENCRYPT_DIR}/scripts/prepare_kat_input.py" ] && [ -f "${ENCRYPT_DIR}/run.sh" ]
}

_build_decrypt() {
    local case_dir="$1"
    local bin_name="$2"
    local stamp="${case_dir}/.roundtrip_built_mode"
    if [ "${ROUNDTRIP_FORCE_REBUILD:-0}" = "1" ]; then
        rm -f "${case_dir}/${bin_name}" "${stamp}"
    fi
    if [ -x "${case_dir}/${bin_name}" ] && [ -d "${case_dir}/out" ] && [ -f "${stamp}" ] \
        && [ "$(cat "${stamp}")" = "${RUN_MODE}" ]; then
        echo "[roundtrip] reuse ${case_dir}/${bin_name} (${RUN_MODE})"
        return 0
    fi
    if [ "${ROUNDTRIP_SKIP_BUILD:-0}" = "1" ]; then
        echo "[roundtrip] ERROR: ROUNDTRIP_SKIP_BUILD=1 但缺少 ${case_dir}/${bin_name}" >&2
        exit 2
    fi
    echo "[roundtrip] cmake build ${case_dir} (${RUN_MODE}) …"
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
    if [ ! -x "${case_dir}/${bin_name}" ]; then
        echo "[roundtrip] ERROR: build 后仍无 ${case_dir}/${bin_name}" >&2
        exit 2
    fi
}

_run_decrypt_kernel() {
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
    /usr/bin/time -f '[roundtrip wall_sec] %e' \
        bash "${REPO_ROOT}/scripts/kernel-run-timeout.sh" "./${bin_name}" 2>&1 | tee "${case_dir}/output/roundtrip_run_metrics.txt"
    if [ "${RUN_MODE}" = "sim" ]; then
        camodel_sim_collect_stray "${case_dir}"
    fi
}

echo "[roundtrip] KeyGen out: ${KEYGEN_OUT}"
echo "[roundtrip] Encrypt:    ${ENCRYPT_DIR}"
echo "[roundtrip] Decrypt:    ${DECRYPT_DIR}"
echo "[roundtrip] RUN_MODE=${RUN_MODE} SEED_D=${SEED_D}"

# --- Encrypt ---
# m/coins 与 KeyGen 同 SEED_D 派生；ek 来自 device KeyGen
python3 - <<PY
import numpy as np
from pathlib import Path
seed_d = int("${SEED_D}")
rng = np.random.default_rng(seed_d + 991)
tmp = Path("${ENCRYPT_DIR}/output/_roundtrip_m_coins")
tmp.mkdir(parents=True, exist_ok=True)
rng.integers(0, 256, size=32, dtype=np.uint8).tofile(tmp / "m.bin")
rng.integers(0, 256, size=32, dtype=np.uint8).tofile(tmp / "coins.bin")
print(f"[roundtrip] m/coins SEED_D={seed_d}")
PY

M_COINS_DIR="${ENCRYPT_DIR}/output/_roundtrip_m_coins"
M_REF="${M_COINS_DIR}/m.bin"

if _is_exp_encrypt; then
    echo "[roundtrip] === device Encrypt (exp prepare_kat_input + run.sh) ==="
    python3 "${ENCRYPT_DIR}/scripts/prepare_kat_input.py" \
        --ek "${EK_FILE}" \
        --m "${M_REF}" \
        --coins "${M_COINS_DIR}/coins.bin" \
        --case-dir "${ENCRYPT_DIR}"
    # 跳过 gen_data；仍与 host golden/c 对拍（prepare 已写）
    (
        cd "${ENCRYPT_DIR}"
        ENCRYPT_SKIP_GEN_DATA=1 SEED_D="${SEED_D}" \
            bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}"
    )
else
    echo "[roundtrip] === device Encrypt (legacy probe path) ==="
    python3 "${REPO_ROOT}/scripts/roundtrip_pke_prepare.py" \
        --mode encrypt \
        --keygen-out "${KEYGEN_OUT}" \
        --encrypt-dir "${ENCRYPT_DIR}" \
        --seed-d "${SEED_D}"
    M_REF="${ENCRYPT_DIR}/input/m.bin"
    _build_decrypt "${ENCRYPT_DIR}" "ascendc_kernels_bbit"
    _run_decrypt_kernel "${ENCRYPT_DIR}" "ascendc_kernels_bbit"
fi

C_DEVICE="${ENCRYPT_DIR}/output/c.bin"
if [ ! -f "${C_DEVICE}" ] || [ "$(wc -c <"${C_DEVICE}")" -ne 1568 ]; then
    echo "[roundtrip] ERROR: Encrypt 未产出 1568B c.bin" >&2
    exit 3
fi
echo "[roundtrip] device c.bin OK ($(wc -c <"${C_DEVICE}") bytes)"

# --- Decrypt ---
_build_decrypt "${DECRYPT_DIR}" "ascendc_kernels_bbit"
python3 "${REPO_ROOT}/scripts/roundtrip_pke_prepare.py" \
    --mode decrypt \
    --keygen-out "${KEYGEN_OUT}" \
    --decrypt-dir "${DECRYPT_DIR}" \
    --c-src "${C_DEVICE}" \
    --seed-d "${SEED_D}"

echo "[roundtrip] === device Decrypt (c from Encrypt) ==="
_run_decrypt_kernel "${DECRYPT_DIR}" "ascendc_kernels_bbit"

M_DEVICE="${DECRYPT_DIR}/output/m.bin"
if [ ! -f "${M_DEVICE}" ] || [ "$(wc -c <"${M_DEVICE}")" -ne 32 ]; then
    echo "[roundtrip] ERROR: Decrypt 未产出 32B m.bin" >&2
    exit 4
fi

python3 "${REPO_ROOT}/scripts/roundtrip_pke_verify.py" \
    --m-device "${M_DEVICE}" \
    --m-ref "${M_REF}"

echo "[SUCCESS] round-trip PKE Encrypt→Decrypt (${RUN_MODE}) SEED_D=${SEED_D}"
