#!/usr/bin/env bash
# vendor_sync.sh — 从活跃探针复制 vendored 源（自包含）
# tiling：保留本目录 compute/ntt_u/f203_decrypt_ntt_u_tiling.h（k=4 几何已定型，勿从错误 encrypt 路径覆盖）。
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "${CASE_DIR}/../.." && pwd)"
if [ ! -d "${REPO_ROOT}/library/shared" ]; then
    REPO_ROOT="$(cd "${CASE_DIR}/../../.." && pwd)"
fi
TESTS="${REPO_ROOT}/ascendc-tests"
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

# k=4 workspace 常量：本探针自包含 tiling（对齐 stable-fips203-mlkem-pke-decrypt-k4，非 encrypt ntt_r）
TILING_IN_TREE="${NTT_DST}/f203_decrypt_ntt_u_tiling.h"
if [ ! -f "${TILING_IN_TREE}" ]; then
    STABLE_DEC="${REPO_ROOT}/examples/stable/stable-fips203-mlkem-pke-decrypt-k4"
    copy_file "${STABLE_DEC}/compute/ntt_u/f203_decrypt_ntt_u_tiling.h" "${TILING_IN_TREE}"
else
    echo "[vendor_sync] keep in-tree compute/ntt_u/f203_decrypt_ntt_u_tiling.h"
fi

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
