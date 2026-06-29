#ifndef STAGE123_POLYVEC8_TILING_H
#define STAGE123_POLYVEC8_TILING_H

/**
 * @file tiling.h
 * @brief 8-poly 紧凑 Stage1：[HI₈, LO₈] 共 16 行（Tag5T polyvec8，无插零）。
 *
 * 输入 polyvec [8,256]；输出 NTT/INTT [8,256]。
 * LUT 与 vec-k4-v2 相同 even/odd stacked 布局；NTT/INTT 由 host 写入不同 bin。
 */
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength; /**< n = 256 */
    int32_t kPolys;     /**< 8 */
    /** 0=仅 S1；1=仅 S2；2=仅 S3；3=S1+S2+S3（默认） */
    int32_t mixPass;
};

namespace tiling {

constexpr size_t n = 256;
constexpr size_t halfN = n / 2;
constexpr size_t kK = 8;
constexpr size_t kPolysPerAiv = kK / 2;
constexpr size_t kLimbsPerPoly = 4;

constexpr size_t lutCols = 512;
constexpr size_t lutPlanarCols = halfN;
constexpr size_t lutStackedRows = 512;

/** 紧凑 S0：行 0..7 HI，行 8..15 LO */
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
