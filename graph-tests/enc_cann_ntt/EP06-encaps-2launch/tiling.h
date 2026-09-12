/**
 * EN01-kem256-ntt-port · Tiling / workspace 布局
 *
 * 迁入来源：thirdparty/cann-ntt-author-merged_dsa/tiling.h
 * Kyber 档：max_kernel_bench_tile_kyber=64；workspace 按 bench tile 动态算。
 */
#ifndef __TIILING_H__
#define __TIILING_H__
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength;
    int32_t bench;
    int32_t q;
    int32_t b_k;
    int32_t b_mu;
    int32_t r28;
};

namespace tiling_one {
    constexpr size_t n = 256;
    constexpr int32_t max_kernel_bench_tile = 8;
    constexpr int32_t max_kernel_bench_tile_kyber = 64;
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

    constexpr size_t WorkspaceSizeForBench(size_t bench) {
        return M3 + n * n
             + 4 * n * bench * sizeof(int8_t)
             + 4 * n * 4 * bench * sizeof(int32_t);
    }
}

#endif
