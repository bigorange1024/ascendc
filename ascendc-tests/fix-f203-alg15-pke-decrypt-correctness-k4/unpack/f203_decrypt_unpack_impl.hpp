/**
 * @file f203_decrypt_unpack_impl.hpp
 * @brief G1 设备段：c → u/v（供 g4_full 单 launch 内联调用）。
 */
#ifndef F203_DECRYPT_UNPACK_IMPL_HPP
#define F203_DECRYPT_UNPACK_IMPL_HPP

#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

namespace decrypt_g4 {

constexpr int32_t kUnpackN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kUnpackK = static_cast<int32_t>(F203_DECRYPT_K);
constexpr int32_t kUnpackQ = static_cast<int32_t>(F203_DECRYPT_Q);

__aicore__ inline uint32_t decompress_d11_u32(uint32_t u)
{
    return ((u * static_cast<uint32_t>(kUnpackQ)) + 1024u) >> 11;
}

__aicore__ inline uint32_t decompress_d5_u32(uint32_t u)
{
    return ((u * static_cast<uint32_t>(kUnpackQ)) + 16u) >> 5;
}

__aicore__ inline void byte_decode_bits_scalar(int32_t *out, const uint8_t *in, uint32_t dBits)
{
    uint32_t bitPos = 0U;
    const uint32_t mask = (dBits >= 32U) ? 0xFFFFFFFFu : ((1U << dBits) - 1U);
    for (uint32_t i = 0; i < static_cast<uint32_t>(kUnpackN); ++i) {
        uint32_t a = 0U;
        for (uint32_t j = 0; j < dBits; ++j) {
            const uint32_t byteIdx = bitPos >> 3;
            const uint32_t bitIdx = bitPos & 7U;
            if ((in[byteIdx] >> bitIdx) & 1U) {
                a |= (1U << j);
            }
            ++bitPos;
        }
        out[i] = static_cast<int32_t>(a & mask);
    }
}

__aicore__ inline void unpack_poly_u11(int32_t *polyOut, const uint8_t *cPoly)
{
    int32_t comp[kUnpackN];
    byte_decode_bits_scalar(comp, cPoly, 11U);
    for (int32_t i = 0; i < kUnpackN; ++i) {
        polyOut[i] = static_cast<int32_t>(decompress_d11_u32(static_cast<uint32_t>(comp[i])));
    }
}

__aicore__ inline void unpack_poly_v5(int32_t *polyOut, const uint8_t *cPoly)
{
    int32_t comp[kUnpackN];
    byte_decode_bits_scalar(comp, cPoly, 5U);
    for (int32_t i = 0; i < kUnpackN; ++i) {
        polyOut[i] = static_cast<int32_t>(decompress_d5_u32(static_cast<uint32_t>(comp[i])));
    }
}

/** c GM → u/v GM；须在 AIV blockIdx==0 上调用。 */
__aicore__ inline void unpack_c_impl(GM_ADDR cGm, GM_ADDR uGm, GM_ADDR vGm)
{
    const auto *cIn = reinterpret_cast<const __gm__ uint8_t *>(cGm);
    auto *uOut = reinterpret_cast<__gm__ int32_t *>(uGm);
    auto *vOut = reinterpret_cast<__gm__ int32_t *>(vGm);

    int32_t uLocal[kUnpackK * kUnpackN];
    int32_t vLocal[kUnpackN];
    for (int32_t p = 0; p < kUnpackK; ++p) {
        uint8_t cPolyLocal[F203_C1_POLY_BYTES];
        const uint32_t cOff = static_cast<uint32_t>(p) * F203_C1_POLY_BYTES;
        for (uint32_t b = 0; b < F203_C1_POLY_BYTES; ++b) {
            cPolyLocal[b] = cIn[cOff + b];
        }
        unpack_poly_u11(uLocal + p * kUnpackN, cPolyLocal);
    }
    uint8_t c2Local[F203_C2_BYTES];
    for (uint32_t b = 0; b < F203_C2_BYTES; ++b) {
        c2Local[b] = cIn[F203_C1_BYTES + b];
    }
    unpack_poly_v5(vLocal, c2Local);

    for (int32_t i = 0; i < kUnpackK * kUnpackN; ++i) {
        uOut[i] = uLocal[i];
    }
    for (int32_t i = 0; i < kUnpackN; ++i) {
        vOut[i] = vLocal[i];
    }
}

} // namespace decrypt_g4

#endif
