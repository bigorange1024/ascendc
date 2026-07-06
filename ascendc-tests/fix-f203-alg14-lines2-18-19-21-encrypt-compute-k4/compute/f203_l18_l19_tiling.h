#ifndef F203_ENCRYPT_L18_L19_TILING_H
#define F203_ENCRYPT_L18_L19_TILING_H

/**
 * @file f203_l18_l19_tiling.h
 * @brief Alg.14 行 18–19 可行性探针：k=4 polyvec，MIX 1×AIC + 2×AIV。
 *
 * workspace 布局：NTT LUT @ 标准偏移；INTT LUT 接在 mat_c 平面之后（单 launch 双阶段 MMAD）。
 */
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength; /**< n = 256 */
    int32_t kPolys;     /**< 4 */
    int32_t mixPass;    /**< 保留；可行性核固定全链 */
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
constexpr size_t lutEvenOddBytes = lutStackedRows * lutPlanarCols;

constexpr size_t s0RowsLogic = 2 * kK;
constexpr size_t mRowsLogic = s0RowsLogic;
constexpr size_t mRows = mRowsLogic;

constexpr size_t kPlanarSlots = kK;
constexpr size_t matCPlanarRows = kPlanarSlots * kLimbsPerPoly * 2;

constexpr size_t LUT_NTT_EVEN_STACKED = 0;
constexpr size_t LUT_NTT_EVEN_TOP = LUT_NTT_EVEN_STACKED;
constexpr size_t LUT_NTT_EVEN_BOTTOM = LUT_NTT_EVEN_STACKED + n * lutPlanarCols;
constexpr size_t LUT_NTT_ODD_STACKED = LUT_NTT_EVEN_BOTTOM + n * lutPlanarCols;
constexpr size_t LUT_NTT_ODD_TOP = LUT_NTT_ODD_STACKED;
constexpr size_t LUT_NTT_ODD_BOTTOM = LUT_NTT_ODD_STACKED + n * lutPlanarCols;
constexpr size_t S0 = LUT_NTT_ODD_BOTTOM + n * lutPlanarCols;

/** 与 stage123 aic/aiv 源文件兼容的 NTT LUT 别名 */
constexpr size_t LUT_EVEN_STACKED = LUT_NTT_EVEN_STACKED;
constexpr size_t LUT_EVEN_TOP = LUT_NTT_EVEN_TOP;
constexpr size_t LUT_EVEN_BOTTOM = LUT_NTT_EVEN_BOTTOM;
constexpr size_t LUT_ODD_STACKED = LUT_NTT_ODD_STACKED;
constexpr size_t LUT_ODD_TOP = LUT_NTT_ODD_TOP;
constexpr size_t LUT_ODD_BOTTOM = LUT_NTT_ODD_BOTTOM;

constexpr size_t matCTmpBytes = mRows * halfN * sizeof(int32_t);
constexpr size_t MAT_C_TMP_LO_EVEN = S0 + mRows * n;
constexpr size_t MAT_C_TMP_LO_ODD = MAT_C_TMP_LO_EVEN + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_EVEN = MAT_C_TMP_LO_ODD + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_ODD = MAT_C_TMP_HI_EVEN + matCTmpBytes;
constexpr size_t MAT_C_PLANAR = MAT_C_TMP_HI_ODD + matCTmpBytes;

constexpr size_t MAT_C = MAT_C_PLANAR;

constexpr size_t wsCoreBytes = MAT_C_PLANAR + matCPlanarRows * halfN * sizeof(int32_t);
constexpr size_t LUT_INTT_EVEN_STACKED = wsCoreBytes;
constexpr size_t LUT_INTT_ODD_STACKED = LUT_INTT_EVEN_STACKED + lutEvenOddBytes;
constexpr size_t wssize = LUT_INTT_ODD_STACKED + lutEvenOddBytes;

constexpr size_t yFileBytes = kK * n * sizeof(int32_t);
constexpr size_t yHatFileBytes = yFileBytes;
constexpr size_t aHatFileBytes = kK * kK * n * sizeof(int32_t);
constexpr size_t uNttFileBytes = kK * n * sizeof(int32_t);
constexpr size_t e1FileBytes = kK * n * sizeof(int32_t);
constexpr size_t uFileBytes = kK * n * sizeof(int32_t);

} // namespace tiling

#endif
