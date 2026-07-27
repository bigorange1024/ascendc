#!/usr/bin/env bash
# pass-fix-f203-compress-decompress-du10-dv4-k2 — ML-KEM-512 W0/B1
#
# 编排子探针：compress/ + decompress/，锁定验收 d∈{4,10}（512 的 d_v / d_u）。
# 算法对照 1024 活跃探针；本树自包含，默认向量路径。
#
# Usage（默认全量 d=4 与 d=10，各跑 compress+decompress）:
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
#
# 调试（非默认）: F203_COMPRESS_DS="10" bash run.sh ...

set -euo pipefail
CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "${CURRENT_DIR}"
_ORIG_ARGS=("$@")

DS="${F203_COMPRESS_DS:-4 10}"
fail=0
for d in ${DS}; do
  echo "===== B1 compress d=${d} ====="
  if ! (cd "${CURRENT_DIR}/compress" && F203_COMPRESS_D="${d}" COMPRESS_D_VEC="${COMPRESS_D_VEC:-1}" bash run.sh "${_ORIG_ARGS[@]}"); then
    echo "[FAIL] compress d=${d}" >&2
    fail=1
  fi
  echo "===== B1 decompress d=${d} ====="
  if ! (cd "${CURRENT_DIR}/decompress" && F203_DECOMPRESS_D="${d}" DECOMPRESS_D_VEC="${DECOMPRESS_D_VEC:-1}" bash run.sh "${_ORIG_ARGS[@]}"); then
    echo "[FAIL] decompress d=${d}" >&2
    fail=1
  fi
done

if [[ "${fail}" -ne 0 ]]; then
  echo "[ERROR] B1 compress/decompress matrix failed" >&2
  exit 1
fi
echo "[SUCCESS] pass-fix-f203-compress-decompress-du10-dv4-k2 d∈{${DS}} (${_ORIG_ARGS[*]:--r cpu})"
exit 0
