#!/usr/bin/env bash
# vendor_sync_from_alg15_decrypt.sh — 从 alg15 Decrypt G4 复制 vendored 源
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "${CASE_DIR}/../.." && pwd)"
SRC="${REPO}/ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4"
DST="${CASE_DIR}/vendor/pke_decrypt"

if [ ! -d "${SRC}" ]; then
    echo "[vendor_sync] ERROR: missing ${SRC}" >&2
    exit 1
fi

mkdir -p "${DST}"

rsync -a --delete \
    --exclude 'build/' --exclude 'out/' --exclude 'input/' --exclude 'output/' \
    --exclude 'sim_log/' --exclude 'OPPROF_*' --exclude 'ascendc_kernels_bbit' \
    --exclude 'main_decrypt.cpp' --exclude 'main_decrypt_g4_run.cpp' \
    --exclude 'INTEGRATION_PLAN.md' --exclude 'STATUS.md' --exclude 'SELF_CONTAINED.md' \
    "${SRC}/compute" \
    "${SRC}/unpack" \
    "${SRC}/prep" \
    "${SRC}/cmake" \
    "${SRC}/f203_decrypt_layout.h" \
    "${DST}/"

mkdir -p "${CASE_DIR}/scripts/host_golden"
if [ -d "${SRC}/scripts/host_golden" ]; then
    rsync -a "${SRC}/scripts/host_golden/ntt_lut_bins.py" "${CASE_DIR}/scripts/host_golden/" 2>/dev/null || true
fi

echo "[vendor_sync] OK vendor/pke_decrypt from alg15"
