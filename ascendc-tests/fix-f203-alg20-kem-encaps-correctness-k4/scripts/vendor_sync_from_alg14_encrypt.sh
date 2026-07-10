#!/usr/bin/env bash
# vendor_sync_from_alg14_encrypt.sh — 同步 Alg.14 Encrypt **G5 拼装树**到 vendor/pke_encrypt/
#
# 源：frozen-fix-f203-alg14-pke-encrypt-correctness-k4（正确性任务已完成并冻结）
# 说明：交付算子 stable-fips203-mlkem-pke-encrypt-k4 为优化全链布局，**无** pack/ +
#       main_encrypt_g5_run.cpp，不能作为本探针 drop-in vendor 源。
# 例外：仅 rsync 到本探针 vendor/；禁止作 ENCRYPT_DIR / 跑 CI / 新实现模板。
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "${CASE_DIR}/../.." && pwd)"
SRC="${REPO}/ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4"
DST="${CASE_DIR}/vendor/pke_encrypt"

if [ ! -d "${SRC}" ]; then
    echo "[vendor_sync] ERROR: missing ${SRC}" >&2
    exit 1
fi

mkdir -p "${DST}"

rsync -a --delete \
    --exclude 'build/' --exclude 'out/' --exclude 'input/' --exclude 'output/' \
    --exclude 'sim_log/' --exclude 'OPPROF_*' --exclude 'ascendc_kernels_bbit' \
    --exclude 'main_encrypt.cpp' --exclude 'main_encrypt_g5_run.cpp' \
    --exclude 'INTEGRATION_PLAN.md' --exclude 'STATUS.md' --exclude 'SELF_CONTAINED.md' \
    --exclude 'FROZEN.md' --exclude 'frozen-gates/' \
    "${SRC}/prep" \
    "${SRC}/compute" \
    "${SRC}/pack" \
    "${SRC}/cmake" \
    "${SRC}/f203_encrypt_layout.h" \
    "${SRC}/f203_encrypt_marker_custom.cpp" \
    "${SRC}/data_utils.h" \
    "${DST}/"

mkdir -p "${CASE_DIR}/scripts/host_golden"
rsync -a --delete "${SRC}/scripts/host_golden/" "${CASE_DIR}/scripts/host_golden/"

ln -sfn vendor/pke_encrypt/compute "${CASE_DIR}/compute"
ln -sfn vendor/pke_encrypt/prep "${CASE_DIR}/prep"

echo "[vendor_sync] OK vendor/pke_encrypt from frozen alg14 G5 correctness (not stable Encrypt)"
