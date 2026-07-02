#!/usr/bin/env bash
# vendor_sync_from_alg14_encrypt.sh — 从 alg14 Encrypt G5 复制 vendored 源（无 marker 生产路径）
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "${CASE_DIR}/../.." && pwd)"
SRC="${REPO}/ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4"
DST="${CASE_DIR}/vendor/pke_encrypt"

if [ ! -d "${SRC}" ]; then
    echo "[vendor_sync] ERROR: missing ${SRC}" >&2
    exit 1
fi

mkdir -p "${DST}"

rsync -a --delete \
    --exclude 'build/' --exclude 'out/' --exclude 'input/' --exclude 'output/' \
    --exclude 'sim_log/' --exclude 'OPPROF_*' --exclude 'ascendc_kernels_bbit' \
    --exclude 'main_encrypt.cpp' \
    --exclude 'INTEGRATION_PLAN.md' --exclude 'STATUS.md' --exclude 'SELF_CONTAINED.md' \
    --exclude 'frozen-gates/' \
    --exclude 'f203_encrypt_marker_custom.cpp' \
    "${SRC}/prep" \
    "${SRC}/compute" \
    "${SRC}/pack" \
    "${SRC}/cmake" \
    "${SRC}/main_encrypt_g5_run.cpp" \
    "${SRC}/f203_encrypt_layout.h" \
    "${SRC}/f203_encrypt_g5_run.hpp" \
    "${SRC}/data_utils.h" \
    "${DST}/"

mkdir -p "${CASE_DIR}/scripts/host_golden"
rsync -a --delete "${SRC}/scripts/host_golden/" "${CASE_DIR}/scripts/host_golden/"

ln -sfn vendor/pke_encrypt/compute "${CASE_DIR}/compute"
ln -sfn vendor/pke_encrypt/prep "${CASE_DIR}/prep"

echo "[vendor_sync] OK vendor/pke_encrypt from alg14 (no marker)"
