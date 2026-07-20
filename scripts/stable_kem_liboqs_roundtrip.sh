#!/usr/bin/env bash
# stable_kem_liboqs_roundtrip.sh — stable KEM 三件套 ↔ liboqs 全链对拍（默认 CPU×1 + SIM×1）
#
# 用途：办公室/回归一键跑「stable KeyGen→Encaps→Decaps」相对 liboqs fixture 的交叉验证
# （含 shared-secret agreement 与拒绝路径），种子与 fixture 同源。
#
# 固定路径（不可用 KEYGEN_DIR= 等覆盖；要换探针请直接调 liboqs_kem_vs_ascendc.sh）：
#   examples/stable/stable-fips203-mlkem-kem-keygen-k4
#   examples/stable/stable-fips203-mlkem-kem-encaps-k4
#   examples/stable/stable-fips203-mlkem-kem-decaps-k4
#
# 底层：scripts/liboqs_kem_vs_ascendc.sh（Phase 0–4；Encaps 喂 fixture m.bin）
#
# Usage（默认即生产全量；勿并行多路 SIM）：
#   bash scripts/stable_kem_liboqs_roundtrip.sh
#   SEED_D=20260619 bash scripts/stable_kem_liboqs_roundtrip.sh
#
# 环境（可选）：
#   SEED_D                 默认 20260619（与 liboqs fixture / 三阶段同种子）
#   SOC_VERSION            默认 Ascend910B4
#   SKIP_CPU=1             跳过 CPU
#   SKIP_SIM=1             跳过 SIM
#   SKIP_CLEAN_SIM=1       SIM 前不清 Decaps build_prod_sim（默认会清）
#   LIBOQS_KEM_VS_SKIP_REJECT=1  跳过 Phase 4 拒绝路径
#
# 说明：Decaps 曾将 f203_encrypt_l18_l19 从 kem/ 迁到 compute/；旧 build_prod_sim
# 可能残留幽灵 .o，增量链接报 multiple definition。默认在 SIM 前 rm -rf 该用例
# build_prod_sim / out_prod_sim，保证可重复绿。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VS="${REPO_ROOT}/scripts/liboqs_kem_vs_ascendc.sh"
SOC_VERSION="${SOC_VERSION:-Ascend910B4}"
export SEED_D="${SEED_D:-20260619}"

KEYGEN_DIR="${REPO_ROOT}/examples/stable/stable-fips203-mlkem-kem-keygen-k4"
ENCAPS_DIR="${REPO_ROOT}/examples/stable/stable-fips203-mlkem-kem-encaps-k4"
DECAPS_DIR="${REPO_ROOT}/examples/stable/stable-fips203-mlkem-kem-decaps-k4"

for d in "${KEYGEN_DIR}" "${ENCAPS_DIR}" "${DECAPS_DIR}"; do
    if [[ ! -d "${d}" ]]; then
        echo "[stable_kem_liboqs_rt] ERROR: missing ${d}" >&2
        exit 1
    fi
done
if [[ ! -x "${VS}" && ! -f "${VS}" ]]; then
    echo "[stable_kem_liboqs_rt] ERROR: missing ${VS}" >&2
    exit 1
fi

_run_vs() {
    local mode="$1"
    echo "[stable_kem_liboqs_rt] === ${mode} · SEED_D=${SEED_D} · stable KeyGen+Encaps+Decaps vs liboqs ==="
    KEYGEN_DIR="${KEYGEN_DIR}" \
        ENCAPS_DIR="${ENCAPS_DIR}" \
        DECAPS_DIR="${DECAPS_DIR}" \
        SEED_D="${SEED_D}" \
        bash "${VS}" -r "${mode}" -v "${SOC_VERSION}"
}

_clean_decaps_sim() {
    if [[ "${SKIP_CLEAN_SIM:-0}" == "1" ]]; then
        echo "[stable_kem_liboqs_rt] SKIP_CLEAN_SIM=1：保留 ${DECAPS_DIR}/build_prod_sim"
        return 0
    fi
    echo "[stable_kem_liboqs_rt] 清理 Decaps SIM 构建残留（避开 kem/l18 幽灵 .o）"
    rm -rf "${DECAPS_DIR}/build_prod_sim" "${DECAPS_DIR}/out_prod_sim"
    # 用例根若残留旧二进制，避免误用
    rm -f "${DECAPS_DIR}/ascendc_kem_decaps_bbit" \
        "${DECAPS_DIR}/ascendc_kem_decaps_phase_e_bbit"
}

if [[ "${SKIP_CPU:-0}" == "1" && "${SKIP_SIM:-0}" == "1" ]]; then
    echo "[stable_kem_liboqs_rt] ERROR: SKIP_CPU=1 且 SKIP_SIM=1，无事可做" >&2
    exit 1
fi

echo "[stable_kem_liboqs_rt] REPO=${REPO_ROOT}"
echo "[stable_kem_liboqs_rt] KEYGEN=${KEYGEN_DIR}"
echo "[stable_kem_liboqs_rt] ENCAPS=${ENCAPS_DIR}"
echo "[stable_kem_liboqs_rt] DECAPS=${DECAPS_DIR}"

if [[ "${SKIP_CPU:-0}" != "1" ]]; then
    _run_vs cpu
else
    echo "[stable_kem_liboqs_rt] SKIP_CPU=1"
fi

if [[ "${SKIP_SIM:-0}" != "1" ]]; then
    _clean_decaps_sim
    # SIM_DIRECT=1：办公室默认直跑 CAModel，不走 msprof 全量 profiling
    SIM_DIRECT=1 _run_vs sim
else
    echo "[stable_kem_liboqs_rt] SKIP_SIM=1"
fi

if [[ "${SKIP_CPU:-0}" != "1" && "${SKIP_SIM:-0}" != "1" ]]; then
    echo "[SUCCESS] stable KEM ↔ liboqs：CPU×1 + SIM_DIRECT sim×1 · SEED_D=${SEED_D}"
elif [[ "${SKIP_CPU:-0}" != "1" ]]; then
    echo "[SUCCESS] stable KEM ↔ liboqs：CPU×1 · SEED_D=${SEED_D}"
else
    echo "[SUCCESS] stable KEM ↔ liboqs：SIM_DIRECT sim×1 · SEED_D=${SEED_D}"
fi
