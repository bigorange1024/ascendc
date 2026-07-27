#!/usr/bin/env bash
# 对照 liboqs 头文件核对 ML-KEM-512 长度宏（P0 参数卡自检）。
set -euo pipefail
REPO_ROOT="$(
  _d="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
  while [ "${_d}" != "/" ]; do
    if [ -f "${_d}/AGENTS.md" ] && [ -d "${_d}/scripts" ]; then
      printf '%s\n' "${_d}"; break
    fi
    _d="$(dirname "${_d}")"
  done
)"
HDR="${REPO_ROOT}/thirdparty/liboqs/src/kem/ml_kem/kem_ml_kem.h"
if [ ! -f "${HDR}" ]; then
  echo "[ERROR] missing ${HDR}; run: bash scripts/clone-thirdparty.sh" >&2
  exit 2
fi
check() {
  local macro="$1" expect="$2"
  local got
  got="$(sed -n "s/^#define[[:space:]]\+${macro}[[:space:]]\+\([0-9]\+\).*/\1/p" "${HDR}" | head -1)"
  if [ "${got}" != "${expect}" ]; then
    echo "[FAIL] ${macro}: got='${got}' expect=${expect}" >&2
    exit 1
  fi
  echo "[OK] ${macro}=${got}"
}
check OQS_KEM_ml_kem_512_length_public_key 800
check OQS_KEM_ml_kem_512_length_secret_key 1632
check OQS_KEM_ml_kem_512_length_ciphertext 768
check OQS_KEM_ml_kem_512_length_shared_secret 32
check OQS_KEM_ml_kem_512_length_keypair_seed 64
check OQS_KEM_ml_kem_512_length_encaps_seed 32
echo "[SUCCESS] ML-KEM-512 size macros match parameter card"
