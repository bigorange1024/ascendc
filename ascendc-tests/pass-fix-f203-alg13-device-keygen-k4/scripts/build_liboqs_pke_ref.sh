#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROBE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${PROBE_DIR}/../.." && pwd)"
LIBOQS_ROOT="${LIBOQS_ROOT:-${REPO_ROOT}/thirdparty/liboqs}"
LIBOQS_BUILD="${LIBOQS_BUILD:-${LIBOQS_ROOT}/build}"
OUT_BIN="${SCRIPT_DIR}/liboqs_pke_keygen_ref"
if [ ! -f "${LIBOQS_BUILD}/lib/liboqs.so" ]; then
    echo "[build_liboqs_pke_ref] missing ${LIBOQS_BUILD}/lib/liboqs.so" >&2
    exit 1
fi
gcc -O2 -Wall -Wextra \
    -I"${LIBOQS_BUILD}/include" \
    "${SCRIPT_DIR}/liboqs_pke_keygen_ref.c" \
    -L"${LIBOQS_BUILD}/lib" -Wl,-rpath,"${LIBOQS_BUILD}/lib" \
    -loqs -o "${OUT_BIN}"
echo "[build_liboqs_pke_ref] OK: ${OUT_BIN}"
