#!/usr/bin/env bash
# vendor_sync_from_alg15_decrypt.sh — 从 alg15 Decrypt G4 复制 vendored 源
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "${CASE_DIR}/../.." && pwd)"
SRC="${REPO}/examples/stable/stable-fips203-mlkem-pke-decrypt-k4"
DST="${CASE_DIR}/vendor/pke_decrypt"

if [ ! -d "${SRC}" ]; then
    echo "[vendor_sync] ERROR: missing ${SRC}" >&2
    exit 1
fi

mkdir -p "${DST}"

rsync -a --delete \
    --exclude 'build/' --exclude 'out/' --exclude 'input/' --exclude 'output/' \
    --exclude 'sim_log/' --exclude 'OPPROF_*' --exclude 'ascendc_kernels_bbit' \
    --exclude 'main_decrypt.cpp' --exclude 'main_decrypt_g4_run.cpp' \
    --exclude 'INTEGRATION_PLAN.md' --exclude 'STATUS.md' --exclude 'SELF_CONTAINED.md' \
    "${SRC}/compute" \
    "${SRC}/unpack" \
    "${SRC}/prep" \
    "${SRC}/cmake" \
    "${SRC}/f203_decrypt_layout.h" \
    "${DST}/"

mkdir -p "${CASE_DIR}/scripts/host_golden"
if [ -d "${SRC}/scripts/host_golden" ]; then
    rsync -a "${SRC}/scripts/host_golden/ntt_lut_bins.py" "${CASE_DIR}/scripts/host_golden/" 2>/dev/null || true
fi

# --- 单库合并适配（decaps 专用，每次 sync 后重放）---
# 背景：decaps 把 decrypt + encrypt 合成单设备库（消除 CAModel 单 session 双库 func_key 污染）。
#   decrypt 树 compute/ntt_u/aiv_func.hpp 与 encrypt 树 compute/ntt_r/aiv_func.hpp **同名但内容分歧**
#   （decrypt=NTT+INTT，encrypt=正向 NTT）；单库把两树 -I 混在一条路径上时裸名 #include "aiv_func.hpp"
#   会拉错那棵树的头（CPU 编译即报 namespace tiling 重定义）。其余 20 个同名头逐字节相同、无需处理。
# 做法：把 decrypt 侧改名 dec_aiv_func.hpp，并把其 4 个包含者改为 dec_aiv_func.hpp。
_ntt_u="${DST}/compute/ntt_u"
if [ -f "${_ntt_u}/aiv_func.hpp" ]; then
    mv -f "${_ntt_u}/aiv_func.hpp" "${_ntt_u}/dec_aiv_func.hpp"
fi
while IFS= read -r _f; do
    sed -i 's|#include "aiv_func.hpp"|#include "dec_aiv_func.hpp"|' "${_f}"
done < <(grep -rl '#[[:space:]]*include "aiv_func.hpp"' "${DST}/compute/ntt_u" "${DST}/compute/intt_w" 2>/dev/null || true)

echo "[vendor_sync] OK vendor/pke_decrypt from alg15 (+单库 dec_aiv_func 改名)"
