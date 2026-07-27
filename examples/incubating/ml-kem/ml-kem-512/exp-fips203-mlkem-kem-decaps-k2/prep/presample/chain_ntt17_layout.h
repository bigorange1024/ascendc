// @probe exp-fips203-mlkem-kem-decaps-k2
// @file prep/presample/chain_ntt17_layout.h
// @layer prep
// @role prep/presample：NTT17 链布局常量
// @production_io Encrypt prep：input ek_pke.bin+coins.bin；output a_hat.bin+re.bin；中间 prf 不落盘。
// @launch prep launch: blockDim=2, AIV_ONLY
// @ai_core SIM：0×AIC+2×AIV；双 AIV 并行 Â；block0 独占 PRF+CBD。
// @depends 见文件内 #include
// @verify run.sh CPU+SIM；verify_result.py max_abs_diff=0。


/**
 * @file chain_ntt17_layout.h
 * @brief KeyGen/NTT17 链 workspace 与文件尺寸常量（presample 旁路布局参考）。
 *
 * 流水线位置：历史 chain_ntt17 / KeyGen 全链 GM 布局；Encrypt prep 主路径不直接使用本头，
 * 但 vendoring 的 presample 树仍保留，供对照 kS/kE、LUT stacked、mat_c 平面尺寸。
 *
 * 与 Encrypt prep golden：无直接 I/O；Â/re 尺寸见 f203_encrypt_prep_layout.h。
 */
#pragma once

#include <cstddef>
#include <cstdint>

/** Host tiling 占位（tileLength / kS / mixPass）；本 Encrypt prep 核不用。 */
struct TilingData {
    int32_t tileLength;
    int32_t kS;
    int32_t mixPass;
};

namespace chain_ntt17 {

constexpr size_t n = 256;
constexpr size_t halfN = n / 2;
/** 历史 presample 旁路的 k=3 布局常量；D14 k2 主路径不引用本头，真实尺寸见 f203_encrypt_prep_layout.h。 */
constexpr size_t kS = 3;
constexpr size_t kE = 3;
/** dst：ŝ‖ê 共 2*kS+kE 行（历史命名）。 */
constexpr size_t kDstPolys = 2 * kS + kE;
constexpr size_t kHatK = kS;
constexpr size_t kHatKK = kHatK * kHatK;
constexpr size_t lutStackedRows = 512;
constexpr size_t lutPlanarCols = halfN;
/** Stage1 编码行数（含 ZERO 填充几何）。 */
constexpr size_t mRows = 8 + 2 * (2 * kS) + 2 * (2 * (kE / 2));
constexpr size_t kPlanarSlots = 2 * kS + kE;
constexpr size_t kLimbsPerPoly = 4;
constexpr size_t matCPlanarRows = kPlanarSlots * kLimbsPerPoly * 2;

// ---- workspace 字节偏移（LUT even/odd stacked → s0 → mat_c 四分 → planar）----
constexpr size_t LUT_EVEN_STACKED = 0;
constexpr size_t LUT_EVEN_BOTTOM = LUT_EVEN_STACKED + n * lutPlanarCols;
constexpr size_t LUT_ODD_STACKED = LUT_EVEN_BOTTOM + n * lutPlanarCols;
constexpr size_t LUT_ODD_BOTTOM = LUT_ODD_STACKED + n * lutPlanarCols;
constexpr size_t S0 = LUT_ODD_BOTTOM + n * lutPlanarCols;
constexpr size_t matCTmpBytes = mRows * halfN * sizeof(int32_t);
constexpr size_t MAT_C_TMP_LO_EVEN = S0 + mRows * n;
constexpr size_t MAT_C_TMP_LO_ODD = MAT_C_TMP_LO_EVEN + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_EVEN = MAT_C_TMP_LO_ODD + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_ODD = MAT_C_TMP_HI_EVEN + matCTmpBytes;
constexpr size_t MAT_C_PLANAR = MAT_C_TMP_HI_ODD + matCTmpBytes;

constexpr size_t wssize = MAT_C_PLANAR + matCPlanarRows * halfN * sizeof(int32_t);
constexpr size_t dstFileBytes = kDstPolys * n * sizeof(int32_t);
constexpr size_t tHatFileBytes = kHatK * n * sizeof(int32_t);
constexpr size_t aHatFileBytes = kHatKK * n * sizeof(int32_t);
constexpr size_t matCFileBytes = matCPlanarRows * halfN * sizeof(int32_t);
constexpr size_t s0FileBytes = mRows * n;
constexpr size_t lutEvenOddFileBytes = lutStackedRows * lutPlanarCols;
constexpr size_t kEkSkBytes = kHatK * 384;

}  // namespace chain_ntt17
