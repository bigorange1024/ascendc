#!/usr/bin/env bash
# vendor_sync_from_stable_keygen.sh — 从 stable PKE KeyGen 同步 prep/ 到本探针（仅 prep，不含 compute）
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "${CASE_DIR}/../.." && pwd)"
STABLE="${REPO}/examples/stable/stable-mlkem-f203-pke-keygen-k4"
DST="${CASE_DIR}/prep"

if [ ! -d "${STABLE}/prep" ]; then
    echo "[vendor_sync] ERROR: missing ${STABLE}/prep" >&2
    exit 1
fi

mkdir -p "${DST}"
rsync -a --delete \
    "${STABLE}/prep/" \
    "${DST}/"

mkdir -p "${CASE_DIR}/scripts/prep"
rsync -a --delete "${STABLE}/scripts/prep/" "${CASE_DIR}/scripts/prep/"

echo "[vendor_sync] OK prep/ + scripts/prep/ from stable-mlkem-f203-pke-keygen-k4"
