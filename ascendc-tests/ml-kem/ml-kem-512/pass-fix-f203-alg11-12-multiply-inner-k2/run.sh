#!/usr/bin/env bash
# pass-fix-f203-alg11-12-multiply-inner-k2 — ML-KEM-512 W1/B6
#
# 编排：
#   multiply/ — Alg.11 MultiplyNTTs 单对（与 k 无关）
#   inner/    — 2×2 InnerProduct，blockDim=2，AIV 1+1 行
#
# Usage:
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

set -euo pipefail
CURRENT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
_ORIG_ARGS=("$@")
fail=0

echo "===== B6 multiply (Alg.11) ====="
if ! (cd "${CURRENT_DIR}/multiply" && bash run.sh "${_ORIG_ARGS[@]}"); then
  echo "[FAIL] multiply" >&2
  fail=1
fi

echo "===== B6 inner 2×2 (1+1 AIV) ====="
if ! (cd "${CURRENT_DIR}/inner" && INNERPRODUCT_P_OUT=2 INNERPRODUCT_S_VEC=2 bash run.sh "${_ORIG_ARGS[@]}"); then
  echo "[FAIL] inner" >&2
  fail=1
fi

if [[ "${fail}" -ne 0 ]]; then
  echo "[ERROR] B6 multiply/inner failed" >&2
  exit 1
fi
echo "[SUCCESS] pass-fix-f203-alg11-12-multiply-inner-k2 (${_ORIG_ARGS[*]:--r cpu})"
