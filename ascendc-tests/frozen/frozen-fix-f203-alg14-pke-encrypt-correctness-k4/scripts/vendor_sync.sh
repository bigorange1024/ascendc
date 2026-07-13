#!/usr/bin/env bash
# vendor_sync.sh — 从活跃探针复制 vendored 源到 prep/（自包含，禁止跨探针 #include）
set -euo pipefail

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TESTS="${CASE_DIR}/.."
REPO="${CASE_DIR}/../.."

AHAT_SRC="${TESTS}/pass-fix-f203-alg13-lines3-7-a-hat-k4"
ALG7_SRC="${TESTS}/pass-fix-f203-alg7-sample-ntt-k4"
SE_SRC="${TESTS}/pass-fix-f203-alg13-lines8-15-se-k4"
CBD_SRC="${TESTS}/pass-fix-f203-alg8-cbd-eta2-k4"

AHAT_DST="${CASE_DIR}/prep/a_hat"
ALG7_DST="${CASE_DIR}/prep/a_hat/alg7"
RE_DST="${CASE_DIR}/prep/re"
CBD_DST="${CASE_DIR}/prep/re/cbd"

mkdir -p "${AHAT_DST}" "${ALG7_DST}" "${RE_DST}" "${CBD_DST}"

copy_file() {
    local src="$1" dst="$2"
    cp -f "${src}" "${dst}"
    echo "[vendor_sync] ${dst#${CASE_DIR}/} <- ${src#${TESTS}/}"
}

# --- prep/a_hat：lines3-7 a_hat16 ---
for f in f203_a_hat16_config.h f203_a_hat16_layout.h f203_a_hat16_ub.hpp; do
    copy_file "${AHAT_SRC}/${f}" "${AHAT_DST}/${f}"
done

# --- prep/a_hat/alg7：SampleNTT 依赖 ---
for f in \
    f203_alg7_config.h f203_alg7_layout.h f203_alg7_g.hpp \
    f203_alg7_shake_xof.hpp f203_alg7_d12_vec.hpp \
    f203_alg7_rej_scalar.h f203_alg7_rej_scalar.c f203_alg7_rej_scalar.hpp \
    f203_alg7_rej_compact.hpp f203_alg7_rej_filter.hpp f203_alg7_rej_vec.hpp \
    f203_alg7_compact_lut.h f203_alg7_interleave_rom.h f203_alg7_deinterleave_rom.h; do
    copy_file "${ALG7_SRC}/${f}" "${ALG7_DST}/${f}"
done

# --- prep/re/cbd：Alg.8 η=2（Encrypt η₁=η₂=2 for ml_kem_1024）---
for f in \
    f203_cbd_eta2_config.h f203_cbd_eta2.hpp f203_cbd_eta2_sw_lut.hpp \
    f203_cbd_eta2_ub_io.hpp cbd2_ab_lut.h; do
    copy_file "${CBD_SRC}/${f}" "${CBD_DST}/${f}"
done

# --- prep/re：SE 向量参考（仅 shake/tiling 模式；Encrypt 用 coins 非 seed_d）---
for f in f203_se_stage_config.hpp f203_se_device_keccak.hpp; do
    if [[ -f "${SE_SRC}/${f}" ]]; then
        copy_file "${SE_SRC}/${f}" "${RE_DST}/${f}"
    fi
done

# --- compute/ntt_r：stage123 三段式 NTT（k=4 polyvec，G2）---
NTT_SRC="${TESTS}/pass-fix-f203-stage123-ntt-intt-polyvec8-vec"
NTT_DST="${CASE_DIR}/compute/ntt_r"
NTT_LUT_DST="${NTT_DST}/thirdparty/ntt_onnx/include/mlkem/stable"

mkdir -p "${NTT_DST}" "${NTT_LUT_DST}"

for f in basic.hpp kyber_limb6.hpp ntt_vec.hpp stage1_config.hpp stage3_config.hpp stage3_mod_variants.hpp \
    aic_func.hpp aiv_func.hpp; do
    copy_file "${NTT_SRC}/${f}" "${NTT_DST}/${f}"
done

copy_file "${NTT_SRC}/thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h" \
    "${NTT_LUT_DST}/transpose_mlkem_luts_i8.h"

# k=4 polyvec tiling（自 stage123 k=8 改写）
cat > "${NTT_DST}/f203_ntt_r_tiling.h" <<'EOF'
#ifndef F203_ENCRYPT_NTT_R_TILING_H
#define F203_ENCRYPT_NTT_R_TILING_H

/**
 * @file f203_ntt_r_tiling.h
 * @brief G2：r polyvec k=4 紧凑 Stage1 [HI₄, LO₄] → NTT r̂ [4,256]。
 *
 * 语义对齐 pass-fix-f203-stage123-ntt-intt-polyvec8-vec，k 由 8 缩为 4（Encrypt r̂）。
 * mixPass=3：S1+S2+S3 全链 NTT（无 INTT）。
 */
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength; /**< n = 256 */
    int32_t kPolys;     /**< 4 */
    /** 0=仅 S1；1=仅 S2；2=仅 S3；3=S1+S2+S3（G2 默认） */
    int32_t mixPass;
};

namespace tiling {

constexpr size_t n = 256;
constexpr size_t halfN = n / 2;
constexpr size_t kK = 4;
constexpr size_t kPolysPerAiv = kK / 2;
constexpr size_t kLimbsPerPoly = 4;

constexpr size_t lutCols = 512;
constexpr size_t lutPlanarCols = halfN;
constexpr size_t lutStackedRows = 512;

constexpr size_t s0RowsLogic = 2 * kK;
constexpr size_t mRowsLogic = s0RowsLogic;
constexpr size_t mRowsPad = 0;
constexpr size_t mRows = mRowsLogic;

constexpr size_t kPlanarSlots = kK;
constexpr size_t matCPlanarRows = kPlanarSlots * kLimbsPerPoly * 2;

constexpr size_t LUT_EVEN_STACKED = 0;
constexpr size_t LUT_EVEN_TOP = LUT_EVEN_STACKED;
constexpr size_t LUT_EVEN_BOTTOM = LUT_EVEN_STACKED + n * lutPlanarCols;
constexpr size_t LUT_ODD_STACKED = LUT_EVEN_BOTTOM + n * lutPlanarCols;
constexpr size_t LUT_ODD_TOP = LUT_ODD_STACKED;
constexpr size_t LUT_ODD_BOTTOM = LUT_ODD_STACKED + n * lutPlanarCols;
constexpr size_t S0 = LUT_ODD_BOTTOM + n * lutPlanarCols;

constexpr size_t matCTmpBytes = mRows * halfN * sizeof(int32_t);
constexpr size_t MAT_C_TMP_LO_EVEN = S0 + mRows * n;
constexpr size_t MAT_C_TMP_LO_ODD = MAT_C_TMP_LO_EVEN + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_EVEN = MAT_C_TMP_LO_ODD + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_ODD = MAT_C_TMP_HI_EVEN + matCTmpBytes;
constexpr size_t MAT_C_PLANAR = MAT_C_TMP_HI_ODD + matCTmpBytes;

constexpr size_t MAT_C = MAT_C_PLANAR;

constexpr size_t wssize = MAT_C_PLANAR + matCPlanarRows * halfN * sizeof(int32_t);

constexpr size_t srcFileBytes = kK * n * sizeof(int32_t);
constexpr size_t dstFileBytes = kK * n * sizeof(int32_t);
constexpr size_t matCFileBytes = matCPlanarRows * halfN * sizeof(int32_t);
constexpr size_t s0FileBytes = mRows * n;
constexpr size_t lutEvenOddFileBytes = lutStackedRows * lutPlanarCols;

} // namespace tiling

#endif
EOF
echo "[vendor_sync] compute/ntt_r/f203_ntt_r_tiling.h (k=4) generated"

# aiv_func 改 include 为本地 k=4 tiling
sed -i 's|"tiling.h"|"f203_ntt_r_tiling.h"|g' "${NTT_DST}/aiv_func.hpp"

# --- compute/alg11：MultiplyNTTs / innerproduct 依赖（G3 at_r + t_dot_r）---
ALG11_SRC="${TESTS}/pass-fix-f203-alg11-12-multiplyntts-k4"
IP_SRC="${TESTS}/pass-fix-f203-alg11-12-innerproduct-k4"
ALG11_DST="${CASE_DIR}/compute/alg11"
AT_R5_DST="${CASE_DIR}/compute/at_r5"

mkdir -p "${ALG11_DST}" "${AT_R5_DST}"

for f in \
    multiply_ntts_ub.hpp multiply_ntts_vec.hpp multiply_ntts_config.hpp \
    alg11_ub_load.hpp alg11_rom_tables.h alg11_rom_tables.cpp \
    alg11_gammas.h alg11_fixed_n256.hpp alg11_vec_pipe.hpp tiling.h; do
    copy_file "${ALG11_SRC}/${f}" "${ALG11_DST}/${f}"
done

for f in innerproduct_tiling.h innerproduct_mod.hpp; do
    copy_file "${IP_SRC}/${f}" "${AT_R5_DST}/${f}"
done

echo "[vendor_sync] compute/at_r5 innerproduct headers vendored (旧 at_r/t_dot_r 已冻结/删除)"
echo "[vendor_sync] OK — prep/a_hat + prep/re + compute/ntt_r + compute/alg11 + compute/at_r5 vendored"
