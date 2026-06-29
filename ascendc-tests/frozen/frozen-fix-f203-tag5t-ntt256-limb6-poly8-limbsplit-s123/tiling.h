/** 历史对照：limb 面对半 Stage3（AIV0 仅 C_lo、AIV1 仅 C_hi）。MLKEM 新实现见 fix-f203-tag5t-ntt256-limb6-poly8-polybatch-s123。 */
#ifndef __TIILING_H__
#define __TIILING_H__
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength;
    int32_t kPolys;
    /** 0=S1+S2+S3；1=仅 Split；2=仅 Mmad（需 S0）；3=仅 Merge（需 mat_c） */
    int32_t mixPass;
};

namespace tiling {
constexpr size_t n = 256;
constexpr size_t kPolys = 8;
constexpr size_t lutCols = 512;
constexpr size_t lutHalfCols = n;
constexpr size_t lutStackedRows = 512;
/** Tag5T R^T：行序 [hi0..hi7 | lo0..lo7] */
constexpr size_t mRows = 2 * kPolys;
/** Stage2 输出竖堆：上行 C_lo、下行 C_hi，各 16×256 */
constexpr size_t matCStackedRows = mRows * 2;

/** LUT [512,256] int8：上 256 行 = L^T 左半列，下 256 行 = L^T 右半列 */
constexpr size_t LUT_STACKED = 0;
constexpr size_t LUT_TOP = LUT_STACKED;
constexpr size_t LUT_BOTTOM = LUT_STACKED + n * lutHalfCols;
constexpr size_t S0 = LUT_STACKED + lutStackedRows * lutHalfCols;
constexpr size_t S2 = S0 + mRows * n;
constexpr size_t S3 = S2 + n;
/** Stage2 竖堆 mat_c [32,256] int32：上行 C_lo、下行 C_hi */
constexpr size_t MAT_C = S3 + n;
constexpr size_t wssize = MAT_C + matCStackedRows * n * sizeof(int32_t);

constexpr size_t matCHalfElems = mRows * n;
constexpr size_t matCHalfBytes = matCHalfElems * sizeof(int32_t);
} // namespace tiling

#endif
