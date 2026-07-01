#!/usr/bin/env bash
# liboqs_kem_vs_ascendc.sh — liboqs KEM KeyGen ↔ AscendC 对拍
#
# Usage:
#   bash scripts/liboqs_kem_vs_ascendc.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash scripts/liboqs_kem_vs_ascendc.sh -r sim -v Ascend910B4

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KEM_DIR="${KEM_DIR:-${REPO_ROOT}/ascendc-tests/fix-f203-alg19-kem-keygen-k4}"

RUN_MODE="cpu"
SOC_VERSION="Ascend910B4"
export SEED_D="${SEED_D:-20260619}"
FIXTURE_DIR="${LIBOQS_KEM_FIXTURE_DIR:-${REPO_ROOT}/output/liboqs_kem_fixture/${SEED_D}}"
export KEM_KEYGEN_VERIFY=0
export KERNEL_COMPUTE_BUDGET_SEC="${LIBOQS_KEM_KERNEL_BUDGET_SEC:-900}"

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

echo "[liboqs_kem_vs] SEED_D=${SEED_D} RUN_MODE=${RUN_MODE}"

python3 "${REPO_ROOT}/scripts/liboqs_kem_fixture.py" --seed-d "${SEED_D}" --out-dir "${FIXTURE_DIR}"

export SEED_D
if [ "${RUN_MODE}" = "sim" ]; then
    SIM_DIRECT=1 bash "${KEM_DIR}/run.sh" -r sim -v "${SOC_VERSION}"
else
    bash "${KEM_DIR}/run.sh" -r "${RUN_MODE}" -v "${SOC_VERSION}"
fi

python3 "${REPO_ROOT}/scripts/liboqs_kem_vs_ascendc_verify.py" \
    --fixture-dir "${FIXTURE_DIR}" \
    --ascendc-out "${KEM_DIR}/output"

echo "[SUCCESS] liboqs KEM KeyGen vs AscendC (${RUN_MODE}) SEED_D=${SEED_D}"
