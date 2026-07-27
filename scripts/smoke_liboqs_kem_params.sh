#!/usr/bin/env bash
# smoke_liboqs_kem_params.sh — 无 AscendC：对 512/768/1024 各生成一套 liboqs fixture 自洽
#
# Usage:
#   bash scripts/smoke_liboqs_kem_params.sh
#   MLKEM_PARAMS="512 768" bash scripts/smoke_liboqs_kem_params.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_ROOT="${SMOKE_LIBOQS_OUT:-${REPO_ROOT}/output/smoke_liboqs_kem_params/$$}"
PARAMS="${MLKEM_PARAMS:-512 768 1024}"

mkdir -p "${OUT_ROOT}"
echo "[smoke_liboqs] build ref…"
bash "${REPO_ROOT}/scripts/build_liboqs_kem_ref.sh"

for p in ${PARAMS}; do
    out="${OUT_ROOT}/p${p}"
    echo "[smoke_liboqs] === param=${p} → ${out} ==="
    MLKEM_PARAM="${p}" python3 "${REPO_ROOT}/scripts/liboqs_kem_fixture.py" \
        --param "${p}" --random --out-dir "${out}"
done

echo "[SUCCESS] smoke_liboqs_kem_params OK root=${OUT_ROOT} params=${PARAMS}"
