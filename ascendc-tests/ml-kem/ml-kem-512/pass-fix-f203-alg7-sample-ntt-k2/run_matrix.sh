#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
fail=0
for j in 0 1; do
  for i in 0 1; do
    echo "===== B4 SampleNTT (j,i)=($j,$i) ====="
    if ! (cd "$HERE" && ALG7_POLY_J=$j ALG7_POLY_I=$i bash run.sh "$@"); then
      echo "[FAIL] (j,i)=($j,$i)" >&2
      fail=1
    fi
  done
done
[[ $fail -eq 0 ]] || exit 1
echo "[SUCCESS] B4 2×2 matrix"
