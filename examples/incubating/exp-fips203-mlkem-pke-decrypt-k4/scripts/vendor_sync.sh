#!/usr/bin/env bash
# vendor_sync.sh — 【维护 / 非默认】从活跃探针刷新 vendored 源
# 生产 run.sh 不调用本脚本；本目录已自包含。
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# incubating/exp-* → 仓库根 → ascendc-tests
REPO_ROOT="$(cd "${CASE_DIR}/../../.." && pwd)"
if [ ! -d "${REPO_ROOT}/ascendc-tests" ]; then
    REPO_ROOT="$(cd "${CASE_DIR}/../.." && pwd)"
fi
TESTS="${REPO_ROOT}/ascendc-tests"
ENC="${TESTS}/stable-fips203-mlkem-pke-encrypt-k4"
NTT_SRC="${TESTS}/pass-fix-f203-stage123-ntt-intt-polyvec8-vec"
ALG11_SRC="${TESTS}/pass-fix-f203-alg11-12-multiplyntts-k4"
IP_SRC="${TESTS}/pass-fix-f203-alg11-12-innerproduct-k4"

copy_file() {
    local src="$1" dst="$2"
    mkdir -p "$(dirname "${dst}")"
    cp -f "${src}" "${dst}"
    echo "[vendor_sync] ${dst#${CASE_DIR}/} <- ${src#${TESTS}/}"
}

NTT_DST="${CASE_DIR}/compute/ntt_u"
INTT_DST="${CASE_DIR}/compute/intt_w"
ALG11_DST="${CASE_DIR}/compute/alg11"
SU_DOT_DST="${CASE_DIR}/compute/su_dot"
LUT_DST="${NTT_DST}/thirdparty/ntt_study/include/mlkem/stable"

mkdir -p "${NTT_DST}" "${INTT_DST}" "${ALG11_DST}" "${SU_DOT_DST}" "${LUT_DST}"

for f in basic.hpp kyber_limb6.hpp ntt_vec.hpp stage1_config.hpp stage3_config.hpp stage3_mod_variants.hpp \
    aic_func.hpp aiv_func.hpp; do
    copy_file "${NTT_SRC}/${f}" "${NTT_DST}/${f}"
done
copy_file "${NTT_SRC}/thirdparty/ntt_study/include/mlkem/stable/transpose_mlkem_luts_i8.h" \
    "${LUT_DST}/transpose_mlkem_luts_i8.h"

# k=4 完整 workspace 常量（对齐 encrypt compute/ntt_r/f203_ntt_r_tiling.h）
copy_file "${ENC}/compute/ntt_r/f203_ntt_r_tiling.h" "${NTT_DST}/f203_decrypt_ntt_u_tiling.h"
sed -i \
    -e 's/F203_ENCRYPT_NTT_R_TILING_H/F203_DECRYPT_NTT_U_TILING_H/g' \
    -e 's/f203_ntt_r_tiling/f203_decrypt_ntt_u_tiling/g' \
    "${NTT_DST}/f203_decrypt_ntt_u_tiling.h"

sed -i 's|"tiling.h"|"f203_decrypt_ntt_u_tiling.h"|g' "${NTT_DST}/aiv_func.hpp"

cat > "${INTT_DST}/f203_decrypt_intt_w_tiling.h" <<'EOF'
#ifndef F203_DECRYPT_INTT_W_TILING_H
#define F203_DECRYPT_INTT_W_TILING_H
#include "f203_decrypt_ntt_u_tiling.h"
#endif
EOF
echo "[vendor_sync] compute/intt_w/f203_decrypt_intt_w_tiling.h <- ntt_u tiling wrapper"

for f in multiply_ntts_ub.hpp multiply_ntts_vec.hpp multiply_ntts_config.hpp alg11_ub_load.hpp \
    alg11_rom_tables.h alg11_rom_tables.cpp alg11_gammas.h alg11_fixed_n256.hpp alg11_vec_pipe.hpp tiling.h; do
    copy_file "${ALG11_SRC}/${f}" "${ALG11_DST}/${f}"
done
for f in innerproduct_tiling.h innerproduct_mod.hpp; do
    copy_file "${IP_SRC}/${f}" "${SU_DOT_DST}/${f}"
done

echo "[vendor_sync] OK"
