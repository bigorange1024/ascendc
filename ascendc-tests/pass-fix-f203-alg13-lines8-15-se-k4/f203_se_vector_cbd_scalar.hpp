/**
 * @file f203_se_vector_cbd_scalar.hpp
 * @brief V3 Phase C：标量 SamplePolyCBD_η=2（与 se-device-scalar 同式，生产默认）。
 */
#pragma once

#include "kernel_operator.h"

#include <cstdint>

namespace F203SeVector {

constexpr uint32_t N = 256U;
constexpr uint32_t Q = 3329U;
constexpr uint32_t SRC_ROWS = 8U;

__aicore__ inline uint32_t Load32Le(const uint8_t *buf)
{
    return static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) | (static_cast<uint32_t>(buf[3]) << 24);
}

__aicore__ inline void SamplePolyCbd2Row(int32_t *dst_row, const uint8_t *buf)
{
    for (uint32_t i = 0; i < N / 8U; ++i) {
        const uint32_t t = Load32Le(buf + 4U * i);
        const uint32_t d = (t & 0x55555555U) + ((t >> 1) & 0x55555555U);
        for (uint32_t j = 0; j < 8U; ++j) {
            int32_t a = static_cast<int32_t>((d >> (4U * j + 0U)) & 0x3U);
            int32_t b = static_cast<int32_t>((d >> (4U * j + 2U)) & 0x3U);
            int32_t c = a - b;
            if (c < 0) {
                c += static_cast<int32_t>(Q);
            }
            c %= static_cast<int32_t>(Q);
            dst_row[8U * i + j] = c;
        }
    }
}

__aicore__ inline void BuildSrcFromPrfGm(const __gm__ uint8_t *prf_out_gm, __gm__ int32_t *src_gm)
{
    uint8_t prf_buf[128];
    int32_t row[N];
    for (uint8_t nonce = 0; nonce < SRC_ROWS; ++nonce) {
        const __gm__ uint8_t *prf_row = prf_out_gm + static_cast<uint32_t>(nonce) * 128U;
        for (uint32_t i = 0; i < 128U; ++i) {
            prf_buf[i] = prf_row[i];
        }
        SamplePolyCbd2Row(row, prf_buf);
        const uint32_t base = static_cast<uint32_t>(nonce) * N;
        for (uint32_t i = 0; i < N; ++i) {
            src_gm[base + i] = row[i];
        }
    }
}

}  // namespace F203SeVector
