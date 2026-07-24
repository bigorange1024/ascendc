#!/usr/bin/env bash
# vendor_sync_from_alg14_encrypt.sh — 同步 Alg.14 Encrypt G5 拼装树到 vendor/pke_encrypt/
#
# 源：frozen-fix-f203-alg14-pke-encrypt-correctness-k4（正确性任务已完成并冻结）
# 说明：交付算子 stable-fips203-mlkem-pke-encrypt-k4 为优化全链布局，无 pack/ +
#       main_encrypt_g5_run.cpp，不能作为本探针 drop-in vendor 源。
# 例外：仅同步到本探针 vendor/；禁止作 ENCRYPT_DIR / 跑 CI / 新实现模板。
# Cloud：无 rsync 时走 scripts/cp_sync.sh。
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "${CASE_DIR}/../../.." && pwd)"
# shellcheck source=../../../../scripts/cp_sync.sh
source "${REPO}/scripts/cp_sync.sh"

SRC="${REPO}/ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4"
DST="${CASE_DIR}/vendor/pke_encrypt"

if [ ! -d "${SRC}" ]; then
    echo "[vendor_sync] ERROR: missing ${SRC}" >&2
    exit 1
fi

mkdir -p "${DST}"

cp_sync_items --delete "${DST}" -- \
    "${SRC}/prep" \
    "${SRC}/compute" \
    "${SRC}/pack" \
    "${SRC}/cmake" \
    "${SRC}/f203_encrypt_layout.h" \
    "${SRC}/f203_encrypt_marker_custom.cpp" \
    "${SRC}/data_utils.h"

cp_sync_dir --delete "${SRC}/scripts/host_golden/" "${CASE_DIR}/scripts/host_golden/"

ln -sfn vendor/pke_encrypt/compute "${CASE_DIR}/compute"
ln -sfn vendor/pke_encrypt/prep "${CASE_DIR}/prep"

echo "[vendor_sync] OK vendor/pke_encrypt from frozen alg14 G5 correctness (not stable Encrypt)"
