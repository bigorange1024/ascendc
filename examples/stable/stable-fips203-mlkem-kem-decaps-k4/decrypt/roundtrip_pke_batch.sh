#!/usr/bin/env bash
# PKE Encrypt→Decrypt 闭环批测（本算子作 Decrypt 段）
# 默认 CPU×10 + SIM×1；KeyGen/Encrypt 走 stable，Decrypt 指向本 exp。
#
#   bash roundtrip_pke_batch.sh
#   ROUNDTRIP_CPU_COUNT=10 ROUNDTRIP_SIM_COUNT=1 bash roundtrip_pke_batch.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${ROOT}/../../.." && pwd)"
export DECRYPT_DIR="${DECRYPT_DIR:-${ROOT}}"
exec bash "${REPO_ROOT}/scripts/roundtrip_pke_batch.sh" "$@"
