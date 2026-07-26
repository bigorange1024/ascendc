#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROBE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
_REPO_CAND="$(cd "${PROBE_DIR}/../.." && pwd)"
if [ -d "${_REPO_CAND}/thirdparty/liboqs" ] || [ -d "${_REPO_CAND}/library/shared" ]; then
    REPO_ROOT="${_REPO_CAND}"
else
    REPO_ROOT="$(cd "${PROBE_DIR}/../../.." && pwd)"
fi
LIBOQS_ROOT="${LIBOQS_ROOT:-${REPO_ROOT}/thirdparty/liboqs}"
LIBOQS_BUILD="${LIBOQS_BUILD:-${LIBOQS_ROOT}/build}"
OUT_BIN="${SCRIPT_DIR}/liboqs_pke_keygen_ref"
LIBOQS_LIB="${LIBOQS_BUILD}/lib/liboqs.so"
LIBOQS_LINK=(-L"${LIBOQS_BUILD}/lib" -Wl,-rpath,"${LIBOQS_BUILD}/lib" -loqs)
if [ ! -f "${LIBOQS_LIB}" ]; then
    if [ -f "${LIBOQS_BUILD}/lib/liboqs.a" ]; then
        LIBOQS_LIB="${LIBOQS_BUILD}/lib/liboqs.a"
        LIBOQS_LINK=("${LIBOQS_LIB}" -lpthread)
    else
        echo "[build_liboqs_pke_ref] missing ${LIBOQS_BUILD}/lib/liboqs.so or liboqs.a" >&2
        exit 1
    fi
fi
gcc -O2 -Wall -Wextra \
    -I"${LIBOQS_BUILD}/include" \
    "${SCRIPT_DIR}/liboqs_pke_keygen_ref.c" \
    "${LIBOQS_LINK[@]}" \
    -o "${OUT_BIN}"
echo "[build_liboqs_pke_ref] OK: ${OUT_BIN}"
