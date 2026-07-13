#ifndef __TIILING_H__
#define __TIILING_H__
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength;
};

namespace tiling {
    constexpr size_t n = 256;
    constexpr size_t M0 = 0;
    constexpr size_t M1 = M0 + n * n;
    constexpr size_t M2 = M1 + n * n;
    constexpr size_t M3 = M2 + n * n;

    constexpr size_t S0 = M3 + n * n;
    constexpr size_t S1 = S0 + n;
    constexpr size_t S2 = S1 + n;
    constexpr size_t S3 = S2 + n;

    constexpr size_t A0 = S3 + n;
    constexpr size_t A1 = A0 + n * 4 * sizeof(int32_t);
    constexpr size_t A2 = A1 + n * 4 * sizeof(int32_t);
    constexpr size_t A3 = A2 + n * 4 * sizeof(int32_t);
    constexpr size_t wssize = A3 + n * 4 * sizeof(int32_t);
}

#endif