#!/usr/bin/env bash
# vendor_sync_from_stable_keygen.sh — 从 stable PKE KeyGen 同步 prep/ 到本探针（仅 prep）
# Cloud：无 rsync 时走 scripts/cp_sync.sh。
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# examples/incubating/<name> → 仓根需上溯 3 层
REPO="$(cd "${CASE_DIR}/../../.." && pwd)"
# shellcheck source=../../../../scripts/cp_sync.sh
source "${REPO}/scripts/cp_sync.sh"

STABLE="${REPO}/examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-keygen-k4"
DST="${CASE_DIR}/prep"

if [ ! -d "${STABLE}/prep" ]; then
    echo "[vendor_sync] ERROR: missing ${STABLE}/prep" >&2
    exit 1
fi

cp_sync_dir --delete "${STABLE}/prep/" "${DST}/"
cp_sync_dir --delete "${STABLE}/scripts/prep/" "${CASE_DIR}/scripts/prep/"

echo "[vendor_sync] OK prep/ + scripts/prep/ from stable-fips203-mlkem-pke-keygen-k4"
