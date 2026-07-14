#!/usr/bin/env bash
# vendor_sync_from_alg14_encrypt.sh — 同步 Alg.14 Encrypt G5 到 vendor/pke_encrypt/（无 marker）
#
# 源：frozen-fix-f203-alg14-pke-encrypt-correctness-k4
# 交付 stable Encrypt 布局不兼容（无 pack/main_encrypt_g5_run）；见 alg20 同名脚本说明。
# Cloud：无 rsync 时走 scripts/cp_sync.sh。
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "${CASE_DIR}/../.." && pwd)"
# shellcheck source=../../../scripts/cp_sync.sh
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
    "${SRC}/main_encrypt_g5_run.cpp" \
    "${SRC}/f203_encrypt_layout.h" \
    "${SRC}/f203_encrypt_g5_run.hpp" \
    "${SRC}/data_utils.h"

# 不碰 scripts/host_golden：本探针自有 decode_t_hat 等；LUT 脚本以仓库版本为准（compute→encrypt/ntt_r）。

ln -sfn vendor/pke_encrypt/compute "${CASE_DIR}/compute"
ln -sfn vendor/pke_encrypt/prep "${CASE_DIR}/prep"

echo "[vendor_sync] OK vendor/pke_encrypt from frozen alg14 G5 (no marker; not stable Encrypt)"
