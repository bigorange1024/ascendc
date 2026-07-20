#!/usr/bin/env bash
# stable_kem_liboqs_roundtrip.sh — stable KEM 三件套 ↔ liboqs（先 liboqs 随机字节，再喂 AscendC）
#
# 流程（每次调用新抽随机，除非给了 KEM_SEED_HEX / M_HEX）：
#   0. liboqs_kem_fixture.py --random
#        → urandom 64B kem_seed=d‖z + 32B m
#        → liboqs keypair_derand / encaps_derand / decaps → fixture 向量
#   1. AscendC KeyGen：KEM_KG_EXT_SEED=1，input/kem_seed.bin = 同上 64B
#   2. AscendC Encaps：M_FILE=fixture/m.bin，EK=device ek
#   3. AscendC Decaps accept：dk=device、c=device
#   4. AscendC Decaps reject：dk=device、c=fixture c_bad
#   上述 1–4 对拍 fixture；默认 CPU×1 再 SIM×1（同一套随机字节）。
#
# 固定用例：
#   examples/stable/stable-fips203-mlkem-kem-{keygen,encaps,decaps}-k4
#
# Usage：
#   bash scripts/stable_kem_liboqs_roundtrip.sh
#   KEM_SEED_HEX=… M_HEX=… bash scripts/stable_kem_liboqs_roundtrip.sh   # 定点复现
#
# 环境（可选）：
#   SOC_VERSION / SKIP_CPU=1 / SKIP_SIM=1 / SKIP_CLEAN_SIM=1
#   LIBOQS_KEM_VS_SKIP_REJECT=1
#   STABLE_KEM_RT_DIR   fixture 根，默认 output/stable_kem_liboqs_rt/<stamp>/
#
# 说明：SIM 前默认清 Decaps build_prod_sim（kem→compute 迁文件幽灵 .o）。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURE_PY="${REPO_ROOT}/scripts/liboqs_kem_fixture.py"
VERIFY_PY="${REPO_ROOT}/scripts/liboqs_kem_vs_ascendc_verify.py"
SOC_VERSION="${SOC_VERSION:-Ascend910B4}"

KEYGEN_DIR="${REPO_ROOT}/examples/stable/stable-fips203-mlkem-kem-keygen-k4"
ENCAPS_DIR="${REPO_ROOT}/examples/stable/stable-fips203-mlkem-kem-encaps-k4"
DECAPS_DIR="${REPO_ROOT}/examples/stable/stable-fips203-mlkem-kem-decaps-k4"

for d in "${KEYGEN_DIR}" "${ENCAPS_DIR}" "${DECAPS_DIR}"; do
    if [[ ! -d "${d}" ]]; then
        echo "[stable_kem_liboqs_rt] ERROR: missing ${d}" >&2
        exit 1
    fi
done

if [[ "${SKIP_CPU:-0}" == "1" && "${SKIP_SIM:-0}" == "1" ]]; then
    echo "[stable_kem_liboqs_rt] ERROR: SKIP_CPU=1 且 SKIP_SIM=1，无事可做" >&2
    exit 1
fi

STAMP="$(date +%Y%m%d_%H%M%S)_$$"
FIXTURE_DIR="${STABLE_KEM_RT_DIR:-${REPO_ROOT}/output/stable_kem_liboqs_rt/${STAMP}}"
mkdir -p "${FIXTURE_DIR}"

echo "[stable_kem_liboqs_rt] REPO=${REPO_ROOT}"
echo "[stable_kem_liboqs_rt] fixture=${FIXTURE_DIR}"

# --- Phase 0: 先抽随机字节 → liboqs 出全链向量 ---
_fx_args=(--random --out-dir "${FIXTURE_DIR}")
if [[ -n "${KEM_SEED_HEX:-}" ]]; then
    _fx_args+=(--kem-seed-hex "${KEM_SEED_HEX}")
fi
if [[ -n "${M_HEX:-}" ]]; then
    _fx_args+=(--m-hex "${M_HEX}")
fi
python3 "${FIXTURE_PY}" "${_fx_args[@]}"

KEM_SEED_BIN="${FIXTURE_DIR}/kem_seed.bin"
M_BIN="${FIXTURE_DIR}/m.bin"
if [[ ! -f "${KEM_SEED_BIN}" || ! -f "${M_BIN}" ]]; then
    echo "[stable_kem_liboqs_rt] ERROR: fixture missing kem_seed.bin / m.bin" >&2
    exit 1
fi
echo "[stable_kem_liboqs_rt] liboqs random bytes ready: kem_seed=$(wc -c <"${KEM_SEED_BIN}")B m=$(wc -c <"${M_BIN}")B"

_verify() {
    python3 "${VERIFY_PY}" --stage "$1" --fixture-dir "${FIXTURE_DIR}" --ascendc-out "$2"
}

_clean_decaps_sim() {
    if [[ "${SKIP_CLEAN_SIM:-0}" == "1" ]]; then
        echo "[stable_kem_liboqs_rt] SKIP_CLEAN_SIM=1"
        return 0
    fi
    echo "[stable_kem_liboqs_rt] 清理 Decaps SIM 构建残留"
    rm -rf "${DECAPS_DIR}/build_prod_sim" "${DECAPS_DIR}/out_prod_sim"
    rm -f "${DECAPS_DIR}/ascendc_kem_decaps_bbit" \
        "${DECAPS_DIR}/ascendc_kem_decaps_phase_e_bbit"
}

_run_mode() {
    local mode="$1"
    echo "[stable_kem_liboqs_rt] === ${mode}: AscendC ← 同一套 liboqs 随机字节 ==="

    # Phase 1: KeyGen 吃 kem_seed（旁路 A；与 liboqs keypair_derand 同 64B）
    mkdir -p "${KEYGEN_DIR}/input"
    cp -f "${KEM_SEED_BIN}" "${KEYGEN_DIR}/input/kem_seed.bin"
    echo "[stable_kem_liboqs_rt] Phase 1 KeyGen KEM_KG_EXT_SEED=1"
    (
        cd "${KEYGEN_DIR}" && \
            KEM_KG_EXT_SEED=1 \
            KEM_KEYGEN_VERIFY=0 \
            bash run.sh -r "${mode}" -v "${SOC_VERSION}"
    )
    _verify keygen "${KEYGEN_DIR}/output"

    # Phase 2: Encaps 吃同一 m + device ek
    echo "[stable_kem_liboqs_rt] Phase 2 Encaps M_FILE=fixture/m.bin"
    (
        cd "${ENCAPS_DIR}" && \
            EK_KEM_SRC="${KEYGEN_DIR}/output/ek_kem.bin" \
            M_FILE="${M_BIN}" \
            KEM_ENCAPS_VERIFY=0 \
            bash run.sh -r "${mode}" -v "${SOC_VERSION}"
    )
    _verify encaps "${ENCAPS_DIR}/output"

    # Phase 3: Decaps accept（须同时喂同一次 KeyGen 的 ek+dk，避免 gen_data 回落到旧 stash ek）
    echo "[stable_kem_liboqs_rt] Phase 3 Decaps accept"
    (
        cd "${DECAPS_DIR}" && \
            EK_KEM_SRC="${KEYGEN_DIR}/output/ek_kem.bin" \
            DK_KEM_SRC="${KEYGEN_DIR}/output/dk_kem.bin" \
            C_SRC="${ENCAPS_DIR}/output/c.bin" \
            M_FILE="${M_BIN}" \
            K_ENC_SRC="${FIXTURE_DIR}/K_decaps.bin" \
            KEM_DECAPS_VERIFY=0 \
            bash run.sh -r "${mode}" -v "${SOC_VERSION}"
    )
    _verify decaps "${DECAPS_DIR}/output"

    # Phase 4: Decaps reject
    if [[ "${LIBOQS_KEM_VS_SKIP_REJECT:-0}" != "1" ]]; then
        echo "[stable_kem_liboqs_rt] Phase 4 Decaps reject"
        (
            cd "${DECAPS_DIR}" && \
                EK_KEM_SRC="${KEYGEN_DIR}/output/ek_kem.bin" \
                DK_KEM_SRC="${KEYGEN_DIR}/output/dk_kem.bin" \
                C_SRC="${FIXTURE_DIR}/c_bad.bin" \
                KEM_DECAPS_REJECT=1 \
                KEM_DECAPS_VERIFY=0 \
                bash run.sh -r "${mode}" -v "${SOC_VERSION}"
        )
        _verify reject "${DECAPS_DIR}/output"
    else
        echo "[stable_kem_liboqs_rt] Phase 4 skipped (LIBOQS_KEM_VS_SKIP_REJECT=1)"
    fi

    echo "[stable_kem_liboqs_rt] ${mode} PASS"
}

if [[ "${SKIP_CPU:-0}" != "1" ]]; then
    _run_mode cpu
else
    echo "[stable_kem_liboqs_rt] SKIP_CPU=1"
fi

if [[ "${SKIP_SIM:-0}" != "1" ]]; then
    _clean_decaps_sim
    SIM_DIRECT=1 _run_mode sim
else
    echo "[stable_kem_liboqs_rt] SKIP_SIM=1"
fi

echo "[SUCCESS] stable KEM ↔ liboqs（liboqs 随机字节 → AscendC）fixture=${FIXTURE_DIR}"
