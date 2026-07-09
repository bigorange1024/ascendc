#!/usr/bin/env bash
# vendor_sync.sh — 历史脚本（本目录已冻结，禁止作活跃 vendor 源）
# Encrypt 继任：examples/stable/stable-fips203-mlkem-pke-encrypt-k4
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "${CASE_DIR}/../../.." && pwd)"
TESTS="${REPO_ROOT}/ascendc-tests"
ENC="${REPO_ROOT}/examples/stable/stable-fips203-mlkem-pke-encrypt-k4"
NTT_SRC="${TESTS}/pass-fix-f203-stage123-ntt-intt-polyvec8-vec"
ALG11_SRC="${TESTS}/pass-fix-f203-alg11-12-multiplyntts-k4"
IP_SRC="${TESTS}/pass-fix-f203-alg11-12-innerproduct-k4"

echo "[vendor_sync] WARN: ${CASE_DIR} is FROZEN — do not use as active vendor source" >&2
echo "[vendor_sync] Encrypt successor: ${ENC}"

copy_file() {
    local src="$1" dst="$2"
    mkdir -p "$(dirname "${dst}")"
    cp -f "${src}" "${dst}"
    echo "[vendor_sync] ${dst#${CASE_DIR}/} <- ${src#${REPO_ROOT}/}"
}

NTT_DST="${CASE_DIR}/compute/ntt_u"
ALG11_DST="${CASE_DIR}/compute/alg11"
IP_DST="${CASE_DIR}/compute/su_dot"

# 以下为历史同步逻辑保留；frozen 树默认不应再跑本脚本做交付
if [ ! -d "${ENC}" ]; then
    echo "[vendor_sync] ERROR: missing ${ENC}" >&2
    exit 1
fi

echo "[vendor_sync] OK (frozen archive; ENC=${ENC})"
