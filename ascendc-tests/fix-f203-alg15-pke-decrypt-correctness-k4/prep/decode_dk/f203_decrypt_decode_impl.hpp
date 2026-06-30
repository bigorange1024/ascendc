/**
 * @file f203_decrypt_decode_impl.hpp
 * @brief G2a：dk → ŝ（ByteDecode₁₂×4，g4_full 内联）。
 */
#ifndef F203_DECRYPT_DECODE_IMPL_HPP
#define F203_DECRYPT_DECODE_IMPL_HPP

#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

namespace decrypt_g4 {

constexpr int32_t kDecodeK = static_cast<int32_t>(F203_DECRYPT_K);
constexpr int32_t kDecodeN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kDecodePolyBytes = 384;

__aicore__ inline void poly_byte_decode12_local(int32_t *out, const uint8_t *in)
{
    for (int32_t i = 0; i < kDecodeN / 2; ++i) {
        const int32_t b0 = static_cast<int32_t>(in[3 * i]);
        const int32_t b1 = static_cast<int32_t>(in[3 * i + 1]);
        const int32_t b2 = static_cast<int32_t>(in[3 * i + 2]);
        out[2 * i] = b0 | ((b1 & 0x0F) << 8);
        out[2 * i + 1] = (b1 >> 4) | (b2 << 4);
    }
}

__aicore__ inline void decode_s_hat_impl(GM_ADDR dkGm, GM_ADDR sHatGm)
{
    const auto *dkBytes = reinterpret_cast<const __gm__ uint8_t *>(dkGm);
    auto *sFlat = reinterpret_cast<__gm__ int32_t *>(sHatGm);

    for (int32_t j = 0; j < kDecodeK; ++j) {
        uint8_t polyBuf[kDecodePolyBytes];
        for (int32_t b = 0; b < kDecodePolyBytes; ++b) {
            polyBuf[b] = dkBytes[j * kDecodePolyBytes + b];
        }
        int32_t coeffs[kDecodeN];
        poly_byte_decode12_local(coeffs, polyBuf);
        for (int32_t c = 0; c < kDecodeN; ++c) {
            sFlat[j * kDecodeN + c] = coeffs[c];
        }
    }
}

} // namespace decrypt_g4

#endif
