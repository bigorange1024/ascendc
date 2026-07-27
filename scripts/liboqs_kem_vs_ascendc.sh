#!/usr/bin/env bash
# liboqs_kem_vs_ascendc.sh — liboqs KEM 全链 ↔ AscendC KeyGen/Encaps/Decaps 四阶段对拍
#
# 流程（数据面：device 输出向前喂给下一探针，独立用 liboqs fixture 交叉验证）：
#   0. liboqs 生成 fixture（ek/dk/c/K/K_decaps/c_bad/K_reject）
#   1. KeyGen 探针（SEED_D）           → ek/dk 与 liboqs 比
#   2. Encaps 探针（AscendC ek + m）   → c/K 与 liboqs 比
#   3. Decaps 探针（AscendC dk + AscendC c，合法路径）→ K 与 liboqs K_decaps 比 + shared-secret agreement
#   4. Decaps 探针（AscendC dk + liboqs c_bad，拒绝路径）→ K 与 liboqs K_reject=J(z‖c_bad) 比
#
# 与各探针「内建 KEM_*_VERIFY=1」的区别：本脚本用**单一 liboqs fixture** 做二次独立对拍，
# 并显式断言 KEM 特有的 shared-secret agreement 与拒绝路径差异性。
#
# Usage（CPU / SIM 均为一等入口）：
#   bash scripts/liboqs_kem_vs_ascendc.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash scripts/liboqs_kem_vs_ascendc.sh -r sim -v Ascend910B4
#
# 环境（可选）：
#   SEED_D                   默认 20260619（三阶段与 fixture 必须同种子）
#   MLKEM_PARAM              512|768|1024（默认 1024）
#   LIBOQS_KEM_FIXTURE_DIR   默认 output/liboqs_kem_fixture/<PARAM>/<SEED_D>/
#   LIBOQS_KEM_VS_SKIP_REJECT=1  跳过 Phase 4 拒绝路径（只验合法链）
#
# 注意：SIM 下 Decaps 走 2-session（探针默认），单跑 ~11min；勿与其他 SIM 并行。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KEYGEN_DIR="${KEYGEN_DIR:-${REPO_ROOT}/ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg19-kem-keygen-device-k4}"
ENCAPS_DIR="${ENCAPS_DIR:-${REPO_ROOT}/examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4}"
DECAPS_DIR="${DECAPS_DIR:-${REPO_ROOT}/examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4}"

RUN_MODE="cpu"
SOC_VERSION="Ascend910B4"
export SEED_D="${SEED_D:-20260619}"
MLKEM_PARAM="${MLKEM_PARAM:-1024}"
FIXTURE_DIR="${LIBOQS_KEM_FIXTURE_DIR:-${REPO_ROOT}/output/liboqs_kem_fixture/${MLKEM_PARAM}/${SEED_D}}"

SHORT=r:,v:
LONG=run-mode:,soc-version:
OPTS=$(getopt -a --options "$SHORT" --longoptions "$LONG" -- "$@")
eval set -- "$OPTS"
while :; do
    case "$1" in
    -r | --run-mode) RUN_MODE="$2"; shift 2 ;;
    -v | --soc-version) SOC_VERSION="$2"; shift 2 ;;
    --) shift; break ;;
    *) echo "[liboqs_kem_vs] unknown option $1" >&2; exit 1 ;;
    esac
done

echo "[liboqs_kem_vs] MLKEM_PARAM=${MLKEM_PARAM} SEED_D=${SEED_D} RUN_MODE=${RUN_MODE} fixture=${FIXTURE_DIR}"

# --- Phase 0: liboqs fixture ---
MLKEM_PARAM="${MLKEM_PARAM}" python3 "${REPO_ROOT}/scripts/liboqs_kem_fixture.py" \
    --param "${MLKEM_PARAM}" --seed-d "${SEED_D}" --out-dir "${FIXTURE_DIR}"

_verify() {
    # $1=stage $2=ascendc_out_dir
    python3 "${REPO_ROOT}/scripts/liboqs_kem_vs_ascendc_verify.py" \
        --stage "$1" --fixture-dir "${FIXTURE_DIR}" --ascendc-out "$2"
}

# --- Phase 1: KeyGen ---
echo "[liboqs_kem_vs] === Phase 1: device KeyGen vs liboqs ek/dk ==="
(cd "${KEYGEN_DIR}" && SEED_D="${SEED_D}" KEM_KEYGEN_VERIFY=0 bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}")
_verify keygen "${KEYGEN_DIR}/output"

# --- Phase 2: Encaps（喂 device ek；m 由 SEED_D 在 device 派生，与 fixture 同前缀）---
echo "[liboqs_kem_vs] === Phase 2: device Encaps vs liboqs c/K ==="
(cd "${ENCAPS_DIR}" && SEED_D="${SEED_D}" \
    EK_KEM_SRC="${KEYGEN_DIR}/output/ek_kem.bin" \
    KEM_ENCAPS_VERIFY=0 bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}")
_verify encaps "${ENCAPS_DIR}/output"

# --- Phase 3: Decaps 合法路径（喂 device dk + device c）---
echo "[liboqs_kem_vs] === Phase 3: device Decaps vs liboqs K (accept, agreement) ==="
(cd "${DECAPS_DIR}" && SEED_D="${SEED_D}" \
    DK_KEM_SRC="${KEYGEN_DIR}/output/dk_kem.bin" \
    C_SRC="${ENCAPS_DIR}/output/c.bin" \
    KEM_DECAPS_VERIFY=0 KEM_DECAPS_TAMPER_C=0 bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}")
_verify decaps "${DECAPS_DIR}/output"

# --- Phase 4: Decaps 拒绝路径（喂 device dk + liboqs 篡改密文 c_bad）---
if [ "${LIBOQS_KEM_VS_SKIP_REJECT:-0}" != "1" ]; then
    echo "[liboqs_kem_vs] === Phase 4: device Decaps vs liboqs J(z||c_bad) (reject) ==="
    (cd "${DECAPS_DIR}" && SEED_D="${SEED_D}" \
        DK_KEM_SRC="${KEYGEN_DIR}/output/dk_kem.bin" \
        C_SRC="${FIXTURE_DIR}/c_bad.bin" \
        KEM_DECAPS_VERIFY=0 KEM_DECAPS_TAMPER_C=0 bash run.sh -r "${RUN_MODE}" -v "${SOC_VERSION}")
    _verify reject "${DECAPS_DIR}/output"
else
    echo "[liboqs_kem_vs] Phase 4 拒绝路径已跳过（LIBOQS_KEM_VS_SKIP_REJECT=1）"
fi

echo "[SUCCESS] liboqs KEM vs AscendC KeyGen+Encaps+Decaps(+reject) (${RUN_MODE}) SEED_D=${SEED_D}"
