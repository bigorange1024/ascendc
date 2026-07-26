#!/usr/bin/env bash
# kem_keypair_stash_bootstrap.sh — 固定 (ek, dk) 供 encaps/decaps 分项 kat 复用
#
# 优先从 keygen 探针 output/ 复制；缺失则提示先跑 keygen kat 或生产 keygen。
#
# Usage:
#   bash scripts/kem_keypair_stash_bootstrap.sh
#   KEM_KEYPAIR_STASH=/path/to/stash bash scripts/kem_keypair_stash_bootstrap.sh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STASH="${KEM_KEYPAIR_STASH:-${REPO_ROOT}/output/kem_keypair_stash}"
KEYGEN_OUT="${KEYGEN_DIR:-${REPO_ROOT}/ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg19-kem-keygen-device-k4}/output"
EK_SRC="${KEM_STASH_EK_SRC:-${KEYGEN_OUT}/ek_kem.bin}"
DK_SRC="${KEM_STASH_DK_SRC:-${KEYGEN_OUT}/dk_kem.bin}"

if [ ! -f "${EK_SRC}" ] || [ ! -f "${DK_SRC}" ]; then
    echo "[stash] ERROR: missing ek/dk:" >&2
    echo "  ${EK_SRC}" >&2
    echo "  ${DK_SRC}" >&2
    echo "[stash] Run keygen first, e.g. bash scripts/liboqs_kem_keygen_batch.sh" >&2
    exit 2
fi

mkdir -p "${STASH}"
cp -f "${EK_SRC}" "${STASH}/ek_kem.bin"
cp -f "${DK_SRC}" "${STASH}/dk_kem.bin"
echo "[stash] OK ${STASH}/ek_kem.bin (1568B) + dk_kem.bin (3168B)"
