#!/usr/bin/env bash
# build_liboqs_pke_ref_mlkem1024.sh — 编译 ML-KEM-1024 专用 PKE 黑盒
#
# 源码 / 产物均带 mlkem1024 后缀，避免被误当成 512/768 通用 helper。
# 本阶段不对其他参数组做 PKE liboqs 交叉（见 512 计划 P0-G）。
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
LIBOQS_ROOT="${LIBOQS_ROOT:-${REPO_ROOT}/thirdparty/liboqs}"
LIBOQS_BUILD="${LIBOQS_BUILD:-${LIBOQS_ROOT}/build}"
OUT_BIN="${SCRIPT_DIR}/liboqs_pke_ref_mlkem1024"
LIBOQS_LIB="${LIBOQS_BUILD}/lib/liboqs.so"
LIBOQS_LINK=(-L"${LIBOQS_BUILD}/lib" -Wl,-rpath,"${LIBOQS_BUILD}/lib" -loqs)
if [ ! -f "${LIBOQS_LIB}" ]; then
    if [ -f "${LIBOQS_BUILD}/lib/liboqs.a" ]; then
        LIBOQS_LIB="${LIBOQS_BUILD}/lib/liboqs.a"
        LIBOQS_LINK=("${LIBOQS_LIB}" -lpthread)
    else
        echo "[build_liboqs_pke_ref_mlkem1024] missing ${LIBOQS_BUILD}/lib/liboqs.{so,a}" >&2
        exit 1
    fi
fi
gcc -O2 -Wall -Wextra \
    -I"${LIBOQS_BUILD}/include" \
    "${SCRIPT_DIR}/liboqs_pke_ref_mlkem1024.c" \
    "${LIBOQS_LINK[@]}" \
    -o "${OUT_BIN}"
# 兼容旧路径名：软链 liboqs_pke_ref → 本产物（只读调用方可继续用旧名）
ln -sfn "$(basename "${OUT_BIN}")" "${SCRIPT_DIR}/liboqs_pke_ref"
echo "[build_liboqs_pke_ref_mlkem1024] OK: ${OUT_BIN} (+ symlink liboqs_pke_ref)"
