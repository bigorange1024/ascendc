#!/usr/bin/env bash
# vendor_sync_from_alg15_decrypt.sh — 同步 Alg.15 Decrypt G4（2-launch）到 vendor/pke_decrypt/
#
# 源：frozen-fix-f203-alg15-pke-decrypt-correctness-k4
# 说明：交付 stable Decrypt / pass-fix device-k4 为 1-kernel fused 布局；
#       Decaps Phase-D 仍按 2-launch G4 编排，故 vendor 源必须是 frozen correctness。
# 例外：仅同步到本探针 vendor/；禁止作 DECRYPT_DIR / 跑 CI。
# Cloud：无 rsync 时走 scripts/cp_sync.sh。
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(cd "${CASE_DIR}/../../.." && pwd)"
# shellcheck source=../../../../scripts/cp_sync.sh
source "${REPO}/scripts/cp_sync.sh"

SRC="${REPO}/ascendc-tests/frozen/frozen-fix-f203-alg15-pke-decrypt-correctness-k4"
DST="${CASE_DIR}/vendor/pke_decrypt"

if [ ! -d "${SRC}" ]; then
    echo "[vendor_sync] ERROR: missing ${SRC}" >&2
    exit 1
fi

mkdir -p "${DST}"

cp_sync_items --delete "${DST}" -- \
    "${SRC}/compute" \
    "${SRC}/unpack" \
    "${SRC}/prep" \
    "${SRC}/cmake" \
    "${SRC}/f203_decrypt_layout.h"

# 不覆盖 scripts/host_golden/ntt_lut_bins.py：本探针版本指向 compute/ntt_r（encrypt symlink）；
# frozen decrypt 脚本写 ntt_u，与本目录 compute 布局不符。

# --- 单库合并适配（decaps 专用，每次 sync 后重放）---
_ntt_u="${DST}/compute/ntt_u"
if [ -f "${_ntt_u}/aiv_func.hpp" ]; then
    mv -f "${_ntt_u}/aiv_func.hpp" "${_ntt_u}/dec_aiv_func.hpp"
fi
while IFS= read -r _f; do
    sed -i 's|#include "aiv_func.hpp"|#include "dec_aiv_func.hpp"|' "${_f}"
done < <(grep -rl '#[[:space:]]*include "aiv_func.hpp"' "${DST}/compute/ntt_u" "${DST}/compute/intt_w" 2>/dev/null || true)

echo "[vendor_sync] OK vendor/pke_decrypt from frozen alg15 G4 (+dec_aiv_func；not stable Decrypt)"
