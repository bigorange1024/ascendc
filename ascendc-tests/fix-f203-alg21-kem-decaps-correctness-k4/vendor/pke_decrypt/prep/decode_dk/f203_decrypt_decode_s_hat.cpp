/**
 * @file f203_decrypt_decode_s_hat.cpp
 * @brief G2a：dk_pke GM → s_hat GM（ByteDecode₁₂ ×4）。
 */
#include "kernel_operator.h"
#include "f203_decrypt_layout.h"

using namespace AscendC;

namespace {

constexpr int32_t kK = static_cast<int32_t>(F203_DECRYPT_K);
constexpr int32_t kN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kPolyBytes = 384;
constexpr int32_t kDkBytes = static_cast<int32_t>(F203_DK_PKE_BYTES);

__aicore__ inline void poly_byte_decode12_local(int32_t *out, const uint8_t *in)
{
    for (int32_t i = 0; i < kN / 2; ++i) {
        const int32_t b0 = static_cast<int32_t>(in[3 * i]);
        const int32_t b1 = static_cast<int32_t>(in[3 * i + 1]);
        const int32_t b2 = static_cast<int32_t>(in[3 * i + 2]);
        out[2 * i] = b0 | ((b1 & 0x0F) << 8);
        out[2 * i + 1] = (b1 >> 4) | (b2 << 4);
    }
}

} // namespace

extern "C" __global__ __aicore__ void f203_decrypt_decode_s_hat(GM_ADDR dkGm, GM_ADDR sHatGm)
{
#if defined(ASCENDC_CPU_DEBUG)
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
#else
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    if (AscendC::GetSubBlockNum() == 1) {
        return;
    }
#endif
    if (GetBlockIdx() != 0) {
        return;
    }

    const auto *dkBytes = reinterpret_cast<const __gm__ uint8_t *>(dkGm);
    auto *sFlat = reinterpret_cast<__gm__ int32_t *>(sHatGm);

    for (int32_t j = 0; j < kK; ++j) {
        uint8_t polyBuf[384];
        for (int32_t b = 0; b < kPolyBytes; ++b) {
            polyBuf[b] = dkBytes[j * kPolyBytes + b];
        }
        int32_t coeffs[256];
        poly_byte_decode12_local(coeffs, polyBuf);
        for (int32_t c = 0; c < kN; ++c) {
            sFlat[j * kN + c] = coeffs[c];
        }
    }
    (void)kDkBytes;
}

#ifndef __CCE_KT_TEST__
void f203_decrypt_decode_s_hat_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *dkGm, uint8_t *sHatGm)
{
    f203_decrypt_decode_s_hat<<<blockDim, l2ctrl, stream>>>(dkGm, sHatGm);
}
#endif
