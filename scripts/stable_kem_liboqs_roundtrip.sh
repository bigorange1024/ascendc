#!/usr/bin/env bash
# stable_kem_liboqs_roundtrip.sh — stable KEM 三件套 ↔ liboqs（先 liboqs 随机字节，再喂 AscendC）
#
# 流程（每一 trial 独立抽随机，除非给了 KEM_SEED_HEX / M_HEX——仅首轮定点）：
#   0. liboqs_kem_fixture.py --random
#        → urandom 64B kem_seed=d‖z + 32B m
#        → liboqs keypair_derand / encaps_derand / decaps → fixture 向量
#   1. AscendC KeyGen：KEM_KG_EXT_SEED=1，input/kem_seed.bin = 同上 64B
#   2. AscendC Encaps：M_FILE=fixture/m.bin，EK=device ek
#   3. AscendC Decaps accept：dk=device、c=device
#   4. AscendC Decaps reject：dk=device、c=fixture c_bad
#   上述 1–4 对拍 fixture。
#
# 默认算子：
#   KeyGen / Encaps：stable 无 `-ct`
#   Decaps：stable 无 `-ct`（可用 DECAPS_DIR 指到 `-ct` 专题树对照）
#
# Usage（默认 = CPU×1 + SIM×1，与办公室回归一致）：
#   bash scripts/stable_kem_liboqs_roundtrip.sh
#
# 对照 / 加压（须显式）：
#   CPU_TRIALS=5 SIM_TRIALS=1 bash scripts/stable_kem_liboqs_roundtrip.sh
#   DECAPS_DIR=$PWD/examples/stable/stable-fips203-mlkem-kem-decaps-ct-k4 \
#     CPU_TRIALS=5 SIM_TRIALS=1 bash scripts/stable_kem_liboqs_roundtrip.sh
#
# 定点复现（仅作用于首个 trial 的 fixture；后续 trial 仍 --random）：
#   KEM_SEED_HEX=… M_HEX=… bash scripts/stable_kem_liboqs_roundtrip.sh
#
# 环境（可选）：
#   SOC_VERSION / SKIP_CPU=1 / SKIP_SIM=1 / SKIP_CLEAN_SIM=1
#   LIBOQS_KEM_VS_SKIP_REJECT=1
#   KEYGEN_DIR / ENCAPS_DIR / DECAPS_DIR
#   CPU_TRIALS（默认 1）/ SIM_TRIALS（默认 1）
#   STABLE_KEM_RT_DIR   fixture 根，默认 output/stable_kem_liboqs_rt/<stamp>/
#
# 说明：SIM 前默认清 Decaps build_prod_sim（kem→compute 迁文件幽灵 .o）；
#       勿对同一 DECAPS_DIR 并行多路 SIM。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURE_PY="${REPO_ROOT}/scripts/liboqs_kem_fixture.py"
VERIFY_PY="${REPO_ROOT}/scripts/liboqs_kem_vs_ascendc_verify.py"
SOC_VERSION="${SOC_VERSION:-Ascend910B4}"

KEYGEN_DIR="${KEYGEN_DIR:-${REPO_ROOT}/examples/stable/stable-fips203-mlkem-kem-keygen-k4}"
ENCAPS_DIR="${ENCAPS_DIR:-${REPO_ROOT}/examples/stable/stable-fips203-mlkem-kem-encaps-k4}"
DECAPS_DIR="${DECAPS_DIR:-${REPO_ROOT}/examples/stable/stable-fips203-mlkem-kem-decaps-k4}"

CPU_TRIALS="${CPU_TRIALS:-1}"
SIM_TRIALS="${SIM_TRIALS:-1}"

for d in "${KEYGEN_DIR}" "${ENCAPS_DIR}" "${DECAPS_DIR}"; do
    if [[ ! -d "${d}" ]]; then
        echo "[stable_kem_liboqs_rt] ERROR: missing ${d}" >&2
        exit 1
    fi
done

if ! [[ "${CPU_TRIALS}" =~ ^[0-9]+$ && "${SIM_TRIALS}" =~ ^[0-9]+$ ]]; then
    echo "[stable_kem_liboqs_rt] ERROR: CPU_TRIALS/SIM_TRIALS 须为非负整数" >&2
    exit 1
fi

if [[ "${SKIP_CPU:-0}" == "1" && "${SKIP_SIM:-0}" == "1" ]]; then
    echo "[stable_kem_liboqs_rt] ERROR: SKIP_CPU=1 且 SKIP_SIM=1，无事可做" >&2
    exit 1
fi

STAMP="$(date +%Y%m%d_%H%M%S)_$$"
FIXTURE_ROOT="${STABLE_KEM_RT_DIR:-${REPO_ROOT}/output/stable_kem_liboqs_rt/${STAMP}}"
mkdir -p "${FIXTURE_ROOT}"

echo "[stable_kem_liboqs_rt] REPO=${REPO_ROOT}"
echo "[stable_kem_liboqs_rt] fixture_root=${FIXTURE_ROOT}"
echo "[stable_kem_liboqs_rt] KEYGEN=${KEYGEN_DIR}"
echo "[stable_kem_liboqs_rt] ENCAPS=${ENCAPS_DIR}"
echo "[stable_kem_liboqs_rt] DECAPS=${DECAPS_DIR}"
echo "[stable_kem_liboqs_rt] CPU_TRIALS=${CPU_TRIALS} SIM_TRIALS=${SIM_TRIALS}"

_verify() {
    python3 "${VERIFY_PY}" --stage "$1" --fixture-dir "$2" --ascendc-out "$3"
}

_clean_decaps_sim() {
    if [[ "${SKIP_CLEAN_SIM:-0}" == "1" ]]; then
        echo "[stable_kem_liboqs_rt] SKIP_CLEAN_SIM=1"
        return 0
    fi
    echo "[stable_kem_liboqs_rt] 清理 Decaps SIM 构建残留: ${DECAPS_DIR}"
    rm -rf "${DECAPS_DIR}/build_prod_sim" "${DECAPS_DIR}/out_prod_sim"
    rm -f "${DECAPS_DIR}/ascendc_kem_decaps_bbit" \
        "${DECAPS_DIR}/ascendc_kem_decaps_phase_e_bbit"
}

# 每一 trial：先 liboqs 抽随机 → 再 AscendC 全链对拍同一套字节
_make_fixture() {
    local out_dir="$1"
    local use_pinned="$2" # 1=允许 KEM_SEED_HEX/M_HEX；0=强制纯 random
    mkdir -p "${out_dir}"
    local _fx_args=(--random --out-dir "${out_dir}")
    if [[ "${use_pinned}" == "1" ]]; then
        if [[ -n "${KEM_SEED_HEX:-}" ]]; then
            _fx_args+=(--kem-seed-hex "${KEM_SEED_HEX}")
        fi
        if [[ -n "${M_HEX:-}" ]]; then
            _fx_args+=(--m-hex "${M_HEX}")
        fi
    fi
    python3 "${FIXTURE_PY}" "${_fx_args[@]}"
    if [[ ! -f "${out_dir}/kem_seed.bin" || ! -f "${out_dir}/m.bin" ]]; then
        echo "[stable_kem_liboqs_rt] ERROR: fixture missing kem_seed.bin / m.bin in ${out_dir}" >&2
        exit 1
    fi
    echo "[stable_kem_liboqs_rt] liboqs fixture ready: ${out_dir} kem_seed=$(wc -c <"${out_dir}/kem_seed.bin")B m=$(wc -c <"${out_dir}/m.bin")B"
}

_run_mode_once() {
    local mode="$1"
    local fixture_dir="$2"
    local trial_tag="$3"

    echo "[stable_kem_liboqs_rt] === ${mode} ${trial_tag}: AscendC ← liboqs fixture=${fixture_dir} ==="

    local kem_seed_bin="${fixture_dir}/kem_seed.bin"
    local m_bin="${fixture_dir}/m.bin"

    mkdir -p "${KEYGEN_DIR}/input"
    cp -f "${kem_seed_bin}" "${KEYGEN_DIR}/input/kem_seed.bin"
    echo "[stable_kem_liboqs_rt] Phase 1 KeyGen KEM_KG_EXT_SEED=1"
    (
        cd "${KEYGEN_DIR}" && \
            KEM_KG_EXT_SEED=1 \
            KEM_KEYGEN_VERIFY=0 \
            bash run.sh -r "${mode}" -v "${SOC_VERSION}"
    )
    _verify keygen "${fixture_dir}" "${KEYGEN_DIR}/output"

    echo "[stable_kem_liboqs_rt] Phase 2 Encaps M_FILE=fixture/m.bin"
    (
        cd "${ENCAPS_DIR}" && \
            EK_KEM_SRC="${KEYGEN_DIR}/output/ek_kem.bin" \
            M_FILE="${m_bin}" \
            KEM_ENCAPS_VERIFY=0 \
            bash run.sh -r "${mode}" -v "${SOC_VERSION}"
    )
    _verify encaps "${fixture_dir}" "${ENCAPS_DIR}/output"

    echo "[stable_kem_liboqs_rt] Phase 3 Decaps accept"
    (
        cd "${DECAPS_DIR}" && \
            EK_KEM_SRC="${KEYGEN_DIR}/output/ek_kem.bin" \
            DK_KEM_SRC="${KEYGEN_DIR}/output/dk_kem.bin" \
            C_SRC="${ENCAPS_DIR}/output/c.bin" \
            M_FILE="${m_bin}" \
            K_ENC_SRC="${fixture_dir}/K_decaps.bin" \
            KEM_DECAPS_VERIFY=0 \
            bash run.sh -r "${mode}" -v "${SOC_VERSION}"
    )
    _verify decaps "${fixture_dir}" "${DECAPS_DIR}/output"

    if [[ "${LIBOQS_KEM_VS_SKIP_REJECT:-0}" != "1" ]]; then
        echo "[stable_kem_liboqs_rt] Phase 4 Decaps reject"
        (
            cd "${DECAPS_DIR}" && \
                EK_KEM_SRC="${KEYGEN_DIR}/output/ek_kem.bin" \
                DK_KEM_SRC="${KEYGEN_DIR}/output/dk_kem.bin" \
                C_SRC="${fixture_dir}/c_bad.bin" \
                KEM_DECAPS_REJECT=1 \
                KEM_DECAPS_VERIFY=0 \
                bash run.sh -r "${mode}" -v "${SOC_VERSION}"
        )
        _verify reject "${fixture_dir}" "${DECAPS_DIR}/output"
    else
        echo "[stable_kem_liboqs_rt] Phase 4 skipped (LIBOQS_KEM_VS_SKIP_REJECT=1)"
    fi

    echo "[stable_kem_liboqs_rt] ${mode} ${trial_tag} PASS"
}

_run_trials() {
    local mode="$1"
    local n="$2"
    local i
    if [[ "${n}" -le 0 ]]; then
        echo "[stable_kem_liboqs_rt] ${mode}: 0 trials (skip)"
        return 0
    fi
    for ((i = 1; i <= n; i++)); do
        local tag="${mode}_t${i}"
        local fx="${FIXTURE_ROOT}/${tag}"
        # 仅全局首个 trial 允许定点 HEX；其后一律纯 random
        local pinned=0
        if [[ "${i}" -eq 1 && "${mode}" == "cpu" && ( -n "${KEM_SEED_HEX:-}" || -n "${M_HEX:-}" ) ]]; then
            pinned=1
        elif [[ "${i}" -eq 1 && "${mode}" == "sim" && "${SKIP_CPU:-0}" == "1" && ( -n "${KEM_SEED_HEX:-}" || -n "${M_HEX:-}" ) ]]; then
            pinned=1
        fi
        _make_fixture "${fx}" "${pinned}"
        if [[ "${mode}" == "sim" ]]; then
            SIM_DIRECT=1 _run_mode_once sim "${fx}" "${tag}"
        else
            _run_mode_once cpu "${fx}" "${tag}"
        fi
    done
}

if [[ "${SKIP_CPU:-0}" != "1" ]]; then
    _run_trials cpu "${CPU_TRIALS}"
else
    echo "[stable_kem_liboqs_rt] SKIP_CPU=1"
fi

if [[ "${SKIP_SIM:-0}" != "1" ]]; then
    _clean_decaps_sim
    _run_trials sim "${SIM_TRIALS}"
else
    echo "[stable_kem_liboqs_rt] SKIP_SIM=1"
fi

echo "[SUCCESS] stable KEM ↔ liboqs（每 trial：liboqs 随机 → AscendC）root=${FIXTURE_ROOT}"
echo "[stable_kem_liboqs_rt] DECAPS=${DECAPS_DIR} CPU_TRIALS=${CPU_TRIALS} SIM_TRIALS=${SIM_TRIALS}"
