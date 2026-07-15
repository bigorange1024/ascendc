#!/usr/bin/env bash
# roundtrip_kem_keygen_encaps_decaps.sh — device KeyGen→Encaps→Decaps 纯设备闭环（不借 liboqs）
#
# 分项脚本（推荐，CPU/SIM 分开、各跑一次）：
#   bash scripts/roundtrip_kem_keygen.sh  -r cpu|sim -v Ascend910B4
#   bash scripts/roundtrip_kem_encaps.sh  -r cpu|sim -v Ascend910B4
#   bash scripts/roundtrip_kem_decaps.sh  -r cpu|sim -v Ascend910B4
# stash：output/roundtrip_kem/<cpu|sim>/
#
# 验证 ML-KEM 核心正确性：Decaps(dk, Encaps(ek).c) 的共享秘密 == Encaps(ek).K（shared-secret agreement）。
# 拒绝路径：device 内部篡改 coins（KEM_DECAPS_TAMPER_C=1）使重加密 c'≠c → 隐式拒绝 → K == J(z‖c) 且 != Encaps K。
# 所有 golden 由 device 输出 + FIPS 203 J 自算，全程无外部参考实现。
#
# 数据面：KeyGen 出 ek/dk → Encaps(device ek) 出 c/K_enc → Decaps(device dk + device c) 出 K_dec。
#
# Usage（CPU / SIM 均为一等入口）：
#   bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r sim -v Ascend910B4
#
# 环境（可选）：
#   SEED_D                       默认 20260619（三阶段须同种子）
#   ROUNDTRIP_KEM_SKIP_REJECT=1  跳过拒绝路径（只验合法闭环）
#
# 注意：SIM 下 Decaps 走 2-session（探针默认），合法 + 拒绝各 ~11min；勿与其他 SIM 并行。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KEYGEN_DIR="${KEYGEN_DIR:-${REPO_ROOT}/ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4}"
ENCAPS_DIR="${ENCAPS_DIR:-${REPO_ROOT}/ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4}"
DECAPS_DIR="${DECAPS_DIR:-${REPO_ROOT}/ascendc-tests/fix-f203-alg21-kem-decaps-correctness-k4}"
STASH_DIR="${REPO_ROOT}/output/roundtrip_kem"

RUN_MODE="cpu"
SOC_VERSION="Ascend910B4"
export SEED_D="${SEED_D:-20260619}"

SHORT=r:,v:
LONG=run-mode:,soc-version:
OPTS=$(getopt -a --options "$SHORT" --longoptions "$LONG" -- "$@")
eval set -- "$OPTS"
while :; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    --) shift; break ;;
    *) echo "[roundtrip_kem] unknown option $1" >&2; exit 1 ;;
    esac
done

mkdir -p "${STASH_DIR}"
echo "[roundtrip_kem] SEED_D=${SEED_D} RUN_MODE=${RUN_MODE}"

# --- Phase 1: device KeyGen → ek/dk ---
echo "[roundtrip_kem] === Phase 1: device KeyGen ==="
(cd "${KEYGEN_DIR}" && SEED_D="${SEED_D}" KEM_KEYGEN_VERIFY=0 bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}")
cp -f "${KEYGEN_DIR}/output/dk_kem.bin" "${STASH_DIR}/dk_kem.bin"

# --- Phase 2: device Encaps(device ek) → c/K_enc ---
echo "[roundtrip_kem] === Phase 2: device Encaps ==="
(cd "${ENCAPS_DIR}" && SEED_D="${SEED_D}" \
    EK_KEM_SRC="${KEYGEN_DIR}/output/ek_kem.bin" \
    KEM_ENCAPS_VERIFY=0 bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}")
cp -f "${ENCAPS_DIR}/output/c.bin" "${STASH_DIR}/c.bin"
cp -f "${ENCAPS_DIR}/output/K.bin" "${STASH_DIR}/K_enc.bin"

# --- Phase 3: device Decaps(device dk + device c) 合法路径 → K_dec ---
echo "[roundtrip_kem] === Phase 3: device Decaps (accept) ==="
(cd "${DECAPS_DIR}" && SEED_D="${SEED_D}" \
    DK_KEM_SRC="${KEYGEN_DIR}/output/dk_kem.bin" \
    C_SRC="${STASH_DIR}/c.bin" \
    KEM_DECAPS_VERIFY=0 KEM_DECAPS_TAMPER_C=0 bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}")
cp -f "${DECAPS_DIR}/output/K.bin" "${STASH_DIR}/K_dec.bin"

python3 "${REPO_ROOT}/scripts/roundtrip_kem_verify.py" agree \
    --k-enc "${STASH_DIR}/K_enc.bin" --k-dec "${STASH_DIR}/K_dec.bin"

# --- Phase 4: device Decaps 拒绝路径（内部篡改 coins，输入 c 不变）→ K == J(z||c) ---
if [ "${ROUNDTRIP_KEM_SKIP_REJECT:-0}" != "1" ]; then
    echo "[roundtrip_kem] === Phase 4: device Decaps (reject via KEM_DECAPS_TAMPER_C) ==="
    (cd "${DECAPS_DIR}" && SEED_D="${SEED_D}" \
        DK_KEM_SRC="${KEYGEN_DIR}/output/dk_kem.bin" \
        C_SRC="${STASH_DIR}/c.bin" \
        KEM_DECAPS_VERIFY=0 KEM_DECAPS_TAMPER_C=1 bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}")
    cp -f "${DECAPS_DIR}/output/K.bin" "${STASH_DIR}/K_reject.bin"
    python3 "${REPO_ROOT}/scripts/roundtrip_kem_verify.py" reject \
        --dk "${STASH_DIR}/dk_kem.bin" \
        --c "${STASH_DIR}/c.bin" \
        --k "${STASH_DIR}/K_reject.bin" \
        --k-enc "${STASH_DIR}/K_enc.bin"
else
    echo "[roundtrip_kem] Phase 4 拒绝路径已跳过（ROUNDTRIP_KEM_SKIP_REJECT=1）"
fi

echo "[SUCCESS] round-trip KEM KeyGen→Encaps→Decaps(+reject) (${RUN_MODE}) SEED_D=${SEED_D}"
