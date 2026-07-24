#!/usr/bin/env bash
# vendor_sync_from_stable_keygen.sh — 从 stable PKE KeyGen 复制 vendored 源到 vendor/pke_keygen/
# Cloud 等环境可能无 rsync：经 scripts/cp_sync.sh（有 rsync 则用之，否则 cp -a）。
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "${CASE_DIR}/../../.." && pwd)"
# shellcheck source=../../../../scripts/cp_sync.sh
source "${REPO}/scripts/cp_sync.sh"

STABLE="${REPO}/examples/stable/stable-fips203-mlkem-pke-keygen-k4"
DST="${CASE_DIR}/vendor/pke_keygen"

if [ ! -d "${STABLE}" ]; then
    echo "[vendor_sync] ERROR: missing stable ${STABLE}" >&2
    exit 1
fi

mkdir -p "${DST}"

# prep + compute + 头文件 + cmake 片段（--delete：整树替换，等价旧 rsync --delete 多源拷贝）
cp_sync_items --delete "${DST}" -- \
    "${STABLE}/prep" \
    "${STABLE}/compute" \
    "${STABLE}/cmake/cpu_lib_keygen.cmake" \
    "${STABLE}/cmake/npu_lib_keygen.cmake" \
    "${STABLE}/f203_keygen_layout.h" \
    "${STABLE}/f203_keygen_prep_layout.h" \
    "${STABLE}/f203_keygen_prep_ub.hpp" \
    "${STABLE}/f203_keygen_prep_entry.cpp" \
    "${STABLE}/data_utils.h"

# scripts/prep（Alg.7 ROM）与 scripts/compute（LUT golden）供 run.sh 使用
cp_sync_dir --delete "${STABLE}/scripts/prep/" "${CASE_DIR}/scripts/prep/"
cp_sync_dir --delete "${STABLE}/scripts/compute/" "${CASE_DIR}/scripts/compute/"
mkdir -p "${CASE_DIR}/thirdparty"
if [ -d "${STABLE}/thirdparty/ntt_onnx" ]; then
    cp_sync_dir "${STABLE}/thirdparty/ntt_onnx/" "${CASE_DIR}/thirdparty/ntt_onnx/"
fi

# ROM 生成脚本写入 <探针>/prep/alg7/；链到 vendor 树
ln -sfn "${DST}/prep" "${CASE_DIR}/prep"

echo "[vendor_sync] OK vendor/pke_keygen + scripts/prep + scripts/compute (+ ntt_onnx if present) from stable"
