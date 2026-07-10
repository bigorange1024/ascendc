// Decrypt NTT(u) tiling 常量。
// 流水线：Alg.15 正向 NTT(u) 段。
// 与 golden：布局一致即可。

#ifndef F203_DECRYPT_NTT_U_TILING_H
#define F203_DECRYPT_NTT_U_TILING_H

/**
 * @file f203_decrypt_ntt_u_tiling.h
 * @brief Decrypt NTT/INTT workspace 与几何常量（k=4 polyvec，平面 mat_c）。
 *
 * Stage1 紧凑 [HI₄, LO₄] → S0；Stage2 AIC MMAD；Stage3 平面 pack + RouteA merge。
 * mixPass=3：S1+S2+S3 全量（生产默认）。INTT 复用本头尺寸，仅 LUT 内容不同。
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
