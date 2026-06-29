#ifndef __TIILING_H__
#define __TIILING_H__
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength;
    int32_t kPolys;
};

namespace tiling {
constexpr size_t n = 256;
constexpr size_t kPolys = 2;
/** split 后左矩阵 A [4,256] int8，行序 [hi0,lo0,hi1,lo1] */
constexpr size_t mRows = 2 * kPolys;

constexpr size_t M0 = 0;
constexpr size_t M1 = M0 + n * n;
constexpr size_t M2 = M1 + n * n;
constexpr size_t M3 = M2 + n * n;

constexpr size_t S0 = M3 + n * n;
constexpr size_t S2 = S0 + mRows * n;
constexpr size_t S3 = S2 + n;
constexpr size_t A0 = S3 + n;
constexpr size_t A1 = A0 + mRows * n * sizeof(int32_t);
constexpr size_t wssize = A1 + mRows * n * sizeof(int32_t);
} // namespace tiling

#endif
