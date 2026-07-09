#!/usr/bin/env bash
# roundtrip_pke_batch.sh — Encrypt→Decrypt 闭环批跑（默认 CPU×10 + SIM×1）
#
# Encrypt 默认 exp-mlkem-f203-pke-encrypt-k4；Decrypt 默认 alg15 correctness。
# 每轮随机 SEED_D；KeyGen 密钥须已存在，或 ROUNDTRIP_BOOTSTRAP_KEYGEN=1。
#
#   bash scripts/roundtrip_pke_batch.sh
#   ROUNDTRIP_CPU_COUNT=10 ROUNDTRIP_SIM_COUNT=1 bash scripts/roundtrip_pke_batch.sh
#   ROUNDTRIP_SEEDS="20260619,1,2,..." bash scripts/roundtrip_pke_batch.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CPU_COUNT="${ROUNDTRIP_CPU_COUNT:-10}"
SIM_COUNT="${ROUNDTRIP_SIM_COUNT:-1}"
SOC_VERSION="${ROUNDTRIP_SOC_VERSION:-Ascend910B4}"
LOG="${ROUNDTRIP_LOG:-${REPO_ROOT}/output/roundtrip_pke_batch.log}"

mkdir -p "$(dirname "${LOG}")"
: >"${LOG}"

_rand_seed() {
    python3 -c 'import secrets; print(secrets.randbelow(2**32))'
}

_parse_seeds() {
    local need=$((CPU_COUNT + SIM_COUNT))
    if [ -n "${ROUNDTRIP_SEEDS:-}" ]; then
        IFS=',' read -r -a ALL_SEEDS <<<"${ROUNDTRIP_SEEDS}"
        if [ "${#ALL_SEEDS[@]}" -lt "${need}" ]; then
            echo "[roundtrip_batch] ROUNDTRIP_SEEDS 需 ${need} 个，现有 ${#ALL_SEEDS[@]}" >&2
            exit 2
        fi
        CPU_SEEDS=("${ALL_SEEDS[@]:0:${CPU_COUNT}}")
        SIM_SEEDS=("${ALL_SEEDS[@]:${CPU_COUNT}:${SIM_COUNT}}")
    else
        CPU_SEEDS=()
        SIM_SEEDS=()
        local i
        for ((i = 0; i < CPU_COUNT; i++)); do CPU_SEEDS+=("$(_rand_seed)"); done
        for ((i = 0; i < SIM_COUNT; i++)); do SIM_SEEDS+=("$(_rand_seed)"); done
    fi
}

_one() {
    local mode="$1"
    local seed="$2"
    local label="$3"
    local idx="$4"
    local total="$5"
    echo "[roundtrip_batch] ${label} ${idx}/${total} SEED_D=${seed} mode=${mode}" | tee -a "${LOG}"
    if ! SEED_D="${seed}" bash "${REPO_ROOT}/scripts/roundtrip_pke_encrypt_decrypt.sh" \
        -r "${mode}" -v "${SOC_VERSION}" >>"${LOG}" 2>&1; then
        echo "[roundtrip_batch] FAIL ${label} ${idx}/${total} seed_d=${seed} — see ${LOG}" >&2
        exit 1
    fi
    echo "[roundtrip_batch] ${label} ${idx}/${total} OK seed_d=${seed}"
}

_parse_seeds
echo "[roundtrip_batch] CPU×${CPU_COUNT} + SIM×${SIM_COUNT}" | tee -a "${LOG}"
echo "[roundtrip_batch] cpu seeds: ${CPU_SEEDS[*]}" | tee -a "${LOG}"
echo "[roundtrip_batch] sim seeds: ${SIM_SEEDS[*]}" | tee -a "${LOG}"

# 首轮可 bootstrap KeyGen（固定种子一次即可；后续轮次复用同一密钥对）
# 注意：闭环验的是「该密钥下 Encrypt→Decrypt 恢复 m」，m/coins 随 SEED_D 变；
# KeyGen 密钥可固定（ROUNDTRIP_KEYGEN_SEED_D），与 m 种子解耦。
export ROUNDTRIP_BOOTSTRAP_KEYGEN="${ROUNDTRIP_BOOTSTRAP_KEYGEN:-1}"
KEYGEN_SEED="${ROUNDTRIP_KEYGEN_SEED_D:-20260619}"
KEYGEN_DIR="${KEYGEN_DIR:-${REPO_ROOT}/ascendc-tests/pass-fix-f203-alg13-device-keygen-k4}"
if [ ! -f "${KEYGEN_DIR}/output/ek_pke.bin" ] || [ ! -f "${KEYGEN_DIR}/output/dk_pke.bin" ]; then
    echo "[roundtrip_batch] bootstrap KeyGen SEED_D=${KEYGEN_SEED} (cpu) …" | tee -a "${LOG}"
    (cd "${KEYGEN_DIR}" && SEED_D="${KEYGEN_SEED}" bash run.sh -r cpu -v "${SOC_VERSION}") >>"${LOG}" 2>&1
fi
# 后续单次 roundtrip 不再重复 bootstrap
export ROUNDTRIP_BOOTSTRAP_KEYGEN=0

i=1
for s in "${CPU_SEEDS[@]}"; do
    _one cpu "${s}" CPU "${i}" "${CPU_COUNT}"
    i=$((i + 1))
done
echo "[roundtrip_batch] CPU ${CPU_COUNT}/${CPU_COUNT} OK"

i=1
for s in "${SIM_SEEDS[@]}"; do
    _one sim "${s}" SIM "${i}" "${SIM_COUNT}"
    i=$((i + 1))
done
echo "[roundtrip_batch] SIM ${SIM_COUNT}/${SIM_COUNT} OK"
echo "[SUCCESS] roundtrip batch CPU×${CPU_COUNT} + SIM×${SIM_COUNT}"
