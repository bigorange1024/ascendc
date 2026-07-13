#!/usr/bin/env bash
# vendor_sync_from_stable_keygen.sh — 从 stable PKE KeyGen 复制 vendored 源到 vendor/pke_keygen/
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "${CASE_DIR}/../.." && pwd)"
STABLE="${REPO}/examples/stable/stable-fips203-mlkem-pke-keygen-k4"
DST="${CASE_DIR}/vendor/pke_keygen"

if [ ! -d "${STABLE}" ]; then
    echo "[vendor_sync] ERROR: missing stable ${STABLE}" >&2
    exit 1
fi

mkdir -p "${DST}"

# prep + compute + 头文件 + cmake 片段
rsync -a --delete \
    --exclude 'run.sh' --exclude 'kat_*' --exclude 'STATUS.md' --exclude 'SELF_CONTAINED.md' \
    --exclude '*.tex' --exclude '*.pdf' --exclude 'main_keygen.cpp' \
    --exclude 'scripts/' --exclude 'thirdparty/' --exclude 'build/' --exclude 'out/' \
    --exclude 'input/' --exclude 'output/' \
    "${STABLE}/prep" \
    "${STABLE}/compute" \
    "${STABLE}/cmake/cpu_lib_keygen.cmake" \
    "${STABLE}/cmake/npu_lib_keygen.cmake" \
    "${STABLE}/f203_keygen_layout.h" \
    "${STABLE}/f203_keygen_prep_layout.h" \
    "${STABLE}/f203_keygen_prep_ub.hpp" \
    "${STABLE}/f203_keygen_prep_entry.cpp" \
    "${STABLE}/data_utils.h" \
    "${DST}/"

# scripts/prep（Alg.7 ROM）与 scripts/compute（LUT golden）供 run.sh 使用
mkdir -p "${CASE_DIR}/scripts/prep" "${CASE_DIR}/scripts/compute"
rsync -a --delete "${STABLE}/scripts/prep/" "${CASE_DIR}/scripts/prep/"
rsync -a --delete "${STABLE}/scripts/compute/" "${CASE_DIR}/scripts/compute/"
mkdir -p "${CASE_DIR}/thirdparty"
rsync -a "${STABLE}/thirdparty/ntt_onnx/" "${CASE_DIR}/thirdparty/ntt_onnx/"

# ROM 生成脚本写入 <探针>/prep/alg7/；链到 vendor 树
ln -sfn "${DST}/prep" "${CASE_DIR}/prep"

echo "[vendor_sync] OK vendor/pke_keygen + scripts/prep + scripts/compute + thirdparty/ntt_onnx from stable"
