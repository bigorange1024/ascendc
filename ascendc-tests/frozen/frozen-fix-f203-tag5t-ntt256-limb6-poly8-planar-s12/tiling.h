/**
 * fix-f203-tag5t-ntt256-limb6-poly8-planar-s12 — Stage1+2，平面 mat_c。
 * Stage2：偶/奇 LUT 分乘 (n=128) + AIV 行重排 → mat_c_planar [64,128]；
 *   每 poly 四行 hh|lh|hl|ll，C_lo/C_hi 各 32 行。无 Stage3 Gather。
 */
#ifndef __TIILING_H__
#define __TIILING_H__
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength;
    int32_t kPolys;
    /** 0=S1+S2；1=仅 Stage1；2=仅 Stage2（需 s0_preset） */
    int32_t mixPass;
};

namespace tiling {
constexpr size_t n = 256;
constexpr size_t halfN = n / 2;
constexpr size_t kPolys = 8;
constexpr size_t kPolysPerAiv = 4;
constexpr size_t kAivBatches = kPolys / kPolysPerAiv;
constexpr size_t kLimbsPerPoly = 4;
constexpr size_t lutCols = 512;
constexpr size_t lutPlanarCols = halfN;
constexpr size_t lutStackedRows = 512;
constexpr size_t mRows = 2 * kPolys;
constexpr size_t s0RowsPerAiv = mRows / kAivBatches;
/** 平面 mat_c：8 poly × 4 limb × 2 (C_lo|C_hi) */
constexpr size_t matCPlanarRows = kPolys * kLimbsPerPoly * 2;

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
constexpr size_t wssize = MAT_C_PLANAR + matCPlanarRows * halfN * sizeof(int32_t);

constexpr size_t lutEvenOddFileBytes = lutStackedRows * lutPlanarCols;
} // namespace tiling

#endif
