#!/usr/bin/env bash
# pass-fix-f203-byteencode-decode-d-k2 — ML-KEM-512 W0/B2
#
# 编排：
#   encode/ + decode/ — ByteEncode/Decode_d，验收 d∈{4,10}（512 密文域）
#   encode12/       — ByteEncode₁₂（密钥域；本波仅 encode，decode₁₂ 随 KeyGen 集成）
#
# Usage:
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

set -euo pipefail
CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "${CURRENT_DIR}"
_ORIG_ARGS=("$@")

DS="${F203_BYTE_CODEC_DS:-4 10}"
fail=0
for d in ${DS}; do
  echo "===== B2 encode d=${d} ====="
  if ! (cd "${CURRENT_DIR}/encode" && F203_BYTE_ENCODE_D="${d}" BYTE_ENCODE_D_VEC="${BYTE_ENCODE_D_VEC:-1}" bash run.sh "${_ORIG_ARGS[@]}"); then
    echo "[FAIL] encode d=${d}" >&2
    fail=1
  fi
  echo "===== B2 decode d=${d} ====="
  if ! (cd "${CURRENT_DIR}/decode" && F203_BYTE_DECODE_D="${d}" BYTE_DECODE_D_VEC="${BYTE_DECODE_D_VEC:-1}" bash run.sh "${_ORIG_ARGS[@]}"); then
    echo "[FAIL] decode d=${d}" >&2
    fail=1
  fi
done

echo "===== B2 encode12 (d=12, encode-only) ====="
if ! (cd "${CURRENT_DIR}/encode12" && bash run.sh "${_ORIG_ARGS[@]}"); then
  echo "[FAIL] encode12" >&2
  fail=1
fi

if [[ "${fail}" -ne 0 ]]; then
  echo "[ERROR] B2 byteencode/decode matrix failed" >&2
  exit 1
fi
echo "[SUCCESS] pass-fix-f203-byteencode-decode-d-k2 d∈{${DS}}+encode12 (${_ORIG_ARGS[*]:--r cpu})"
exit 0
