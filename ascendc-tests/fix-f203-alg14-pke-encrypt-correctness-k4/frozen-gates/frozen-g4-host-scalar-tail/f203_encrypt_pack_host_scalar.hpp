#pragma once

/**
 * @file f203_encrypt_pack_host_scalar.hpp
 * @brief G4 pack Host 标量（**已冻结**：frozen-gates/frozen-g4-host-scalar-tail/）。
 * G5 生产路径不得使用；仅 ENCRYPT_GATE=4 过渡回放。
 */
#include "f203_encrypt_layout.h"
#include "f203_encrypt_pack_config.hpp"

#include <cstdint>
#include <cstring>

namespace f203_pack_host {

inline uint32_t compress_d5_u32(uint32_t u)
{
    const uint32_t d0 = u * 1290176u;
    return ((d0 + (1u << 27)) >> 27) & 0x1fu;
}

inline uint32_t compress_d11_u32(uint32_t u)
{
    const uint64_t d0 = static_cast<uint64_t>(u) * 5284526080ull;
    const uint64_t t = (d0 + (static_cast<uint64_t>(1) << 32)) >> 33;
    return static_cast<uint32_t>(t & 0x7ffu);
}

inline void byte_encode_bits(uint8_t *out, const int32_t *comp, uint32_t dBits)
{
    uint32_t bitPos = 0U;
    uint8_t cur = 0U;
    uint32_t outIdx = 0U;
    const uint32_t mask = (dBits >= 32U) ? 0xFFFFFFFFu : ((1U << dBits) - 1U);
    for (uint32_t i = 0; i < static_cast<uint32_t>(F203_ENCRYPT_N); ++i) {
        uint32_t a = static_cast<uint32_t>(comp[i]) & mask;
        for (uint32_t j = 0; j < dBits; ++j) {
            if ((a >> j) & 1U) {
                cur |= static_cast<uint8_t>(1U << (bitPos & 7U));
            }
            ++bitPos;
            if ((bitPos & 7U) == 0U) {
                out[outIdx++] = cur;
                cur = 0U;
            }
        }
    }
    if ((bitPos & 7U) != 0U) {
        out[outIdx] = cur;
    }
}

inline void pack_one_poly_u11(uint8_t *out, const int32_t *polyIn)
{
    int32_t comp[F203_ENCRYPT_N];
    for (int32_t i = 0; i < F203_ENCRYPT_N; ++i) {
        uint32_t u = static_cast<uint32_t>(polyIn[i]);
        if (u >= static_cast<uint32_t>(F203_ENCRYPT_Q)) {
            u = static_cast<uint32_t>(F203_ENCRYPT_Q) - 1U;
        }
        comp[i] = static_cast<int32_t>(compress_d11_u32(u));
    }
    byte_encode_bits(out, comp, 11U);
}

inline void pack_one_poly_v5(uint8_t *out, const int32_t *polyIn)
{
    int32_t comp[F203_ENCRYPT_N];
    for (int32_t i = 0; i < F203_ENCRYPT_N; ++i) {
        uint32_t u = static_cast<uint32_t>(polyIn[i]);
        if (u >= static_cast<uint32_t>(F203_ENCRYPT_Q)) {
            u = static_cast<uint32_t>(F203_ENCRYPT_Q) - 1U;
        }
        comp[i] = static_cast<int32_t>(compress_d5_u32(u));
    }
    byte_encode_bits(out, comp, 5U);
}

/** u[4,256] + v[256] → c.bin 1568B。 */
inline void pack_ciphertext(const uint8_t *u, const uint8_t *v, uint8_t *c_out)
{
    const auto *u32 = reinterpret_cast<const int32_t *>(u);
    const auto *v32 = reinterpret_cast<const int32_t *>(v);
    for (int32_t p = 0; p < F203_ENCRYPT_K; ++p) {
        pack_one_poly_u11(c_out + static_cast<uint32_t>(p) * F203_C1_POLY_BYTES, u32 + p * F203_ENCRYPT_N);
    }
    pack_one_poly_v5(c_out + F203_C1_BYTES, v32);
}

} // namespace f203_pack_host
