/**
 * fix-f203-tag5t-ntt256-limb6-poly8-polybatch-s123 — 本仓 MLKEM Tag5T NTT 权威 tiling。
 * poly-batch：Stage2 后每 AIV 读本批 C_lo+C_hi，握完整 poly；禁止 hi/lo 分核。见 docs/notes/MLKEM-NTT-实现总结.md。
 */
#ifndef __TIILING_H__
#define __TIILING_H__
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength;
    int32_t kPolys;
    /** 0=S1+S2+S3；1=仅 Stage1；2=仅 Stage2（需 S0 preset）；3=仅 Stage3（需 mat_c preset） */
    int32_t mixPass;
};

namespace tiling {
constexpr size_t n = 256;
constexpr size_t kPolys = 8;
/** 每个 AIV subcore 负责的 poly 数（poly-batch 切分） */
constexpr size_t kPolysPerAiv = 4;
constexpr size_t kAivBatches = kPolys / kPolysPerAiv;
constexpr size_t lutCols = 512;
constexpr size_t lutHalfCols = n;
constexpr size_t lutStackedRows = 512;
/** Stage1 左因子 S0 [16,256] int8：poly-batch 行序
 *  [hi0,hi1,hi2,hi3, lo0,lo1,lo2,lo3, hi4,hi5,hi6,hi7, lo4,lo5,lo6,lo7] */
constexpr size_t mRows = 2 * kPolys;
constexpr size_t s0RowsPerAiv = mRows / kAivBatches;
/** Stage2 输出竖堆：C_lo [0:16)、C_hi [16:32)，行序与 S0 对齐 */
constexpr size_t matCStackedRows = mRows * 2;

constexpr size_t LUT_STACKED = 0;
constexpr size_t LUT_TOP = LUT_STACKED;
constexpr size_t LUT_BOTTOM = LUT_STACKED + n * lutHalfCols;
constexpr size_t S0 = LUT_STACKED + lutStackedRows * lutHalfCols;
constexpr size_t S2 = S0 + mRows * n;
constexpr size_t S3 = S2 + n;
constexpr size_t MAT_C = S3 + n;
constexpr size_t wssize = MAT_C + matCStackedRows * n * sizeof(int32_t);

constexpr size_t matCHalfElems = mRows * n;
constexpr size_t matCHalfBytes = matCHalfElems * sizeof(int32_t);
} // namespace tiling

#endif
