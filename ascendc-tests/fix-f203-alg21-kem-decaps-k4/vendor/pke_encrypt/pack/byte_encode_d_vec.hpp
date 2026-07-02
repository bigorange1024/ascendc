#ifndef BYTE_ENCODE_D_VEC_HPP
#define BYTE_ENCODE_D_VEC_HPP

#include "byte_encode_d_config.hpp"
#include "f203_mlkem_params.h"
#include "kernel_operator.h"

namespace byte_encode_d {

constexpr uint32_t kPolyLen = static_cast<uint32_t>(F203_MLKEM_N);
constexpr uint32_t kOutBytes = static_cast<uint32_t>(F203_BYTE_ENCODE_POLY_BYTES);

__aicore__ inline void mask_low_bits_i32(AscendC::LocalTensor<int32_t> &v, AscendC::LocalTensor<int32_t> &tmp,
                                         int32_t bits, uint32_t count)
{
    using AscendC::Muls;
    using AscendC::ShiftRight;
    using AscendC::Sub;
    const int32_t n = static_cast<int32_t>(count);
    const int32_t scale = static_cast<int32_t>(1) << bits;
    ShiftRight(tmp, v, bits, n);
    Muls(tmp, tmp, scale, n);
    Sub(v, v, tmp, n);
}

/** FIPS 203 Alg.5 标量比特串行化（d=5/11 等）。 */
__aicore__ inline void poly_byte_encode_bits_local(AscendC::LocalTensor<uint8_t> &out,
                                                   AscendC::LocalTensor<int32_t> &in, uint32_t dBits)
{
    uint32_t bitPos = 0U;
    uint8_t cur = 0U;
    uint32_t outIdx = 0U;
    const uint32_t mask = (dBits >= 32U) ? 0xFFFFFFFFu : ((1U << dBits) - 1U);
    for (uint32_t i = 0; i < kPolyLen; ++i) {
        uint32_t a = static_cast<uint32_t>(in.GetValue(static_cast<int32_t>(i)) & static_cast<int32_t>(mask));
        for (uint32_t j = 0; j < dBits; ++j) {
            if ((a >> j) & 1U) {
                cur |= static_cast<uint8_t>(1U << (bitPos & 7U));
            }
            ++bitPos;
            if ((bitPos & 7U) == 0U) {
                out.SetValue(outIdx++, cur);
                cur = 0U;
            }
        }
    }
    if ((bitPos & 7U) != 0U) {
        out.SetValue(outIdx, cur);
    }
}

#if F203_BYTE_ENCODE_D == 4

__aicore__ inline void pack_d4_group(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                     uint32_t group)
{
    const uint32_t base = group * 8U;
    const uint8_t t0 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 0U)) & 0xF);
    const uint8_t t1 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 1U)) & 0xF);
    const uint8_t t2 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 2U)) & 0xF);
    const uint8_t t3 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 3U)) & 0xF);
    const uint8_t t4 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 4U)) & 0xF);
    const uint8_t t5 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 5U)) & 0xF);
    const uint8_t t6 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 6U)) & 0xF);
    const uint8_t t7 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 7U)) & 0xF);
    const uint32_t byteBase = group * 4U;
    out.SetValue(byteBase + 0U, static_cast<uint8_t>(t0 | (t1 << 4)));
    out.SetValue(byteBase + 1U, static_cast<uint8_t>(t2 | (t3 << 4)));
    out.SetValue(byteBase + 2U, static_cast<uint8_t>(t4 | (t5 << 4)));
    out.SetValue(byteBase + 3U, static_cast<uint8_t>(t6 | (t7 << 4)));
}

__aicore__ inline void poly_byte_encode_d4_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                                 AscendC::LocalTensor<int32_t> &tmp)
{
    mask_low_bits_i32(in, tmp, 4, kPolyLen);
    AscendC::PipeBarrier<PIPE_ALL>();
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        pack_d4_group(out, in, g);
    }
}

#elif F203_BYTE_ENCODE_D == 5

__aicore__ inline void poly_byte_encode_d5_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                                 AscendC::LocalTensor<int32_t> &)
{
    poly_byte_encode_bits_local(out, in, 5U);
}

#elif F203_BYTE_ENCODE_D == 10

__aicore__ inline void pack_d10_group(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                      uint32_t group)
{
    const uint32_t base = group * 4U;
    const uint16_t t0 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 0U)) & 0x3FF);
    const uint16_t t1 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 1U)) & 0x3FF);
    const uint16_t t2 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 2U)) & 0x3FF);
    const uint16_t t3 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 3U)) & 0x3FF);
    const uint32_t byteBase = group * 5U;
    out.SetValue(byteBase + 0U, static_cast<uint8_t>((t0 >> 0) & 0xFF));
    out.SetValue(byteBase + 1U, static_cast<uint8_t>((t0 >> 8) | ((t1 << 2) & 0xFF)));
    out.SetValue(byteBase + 2U, static_cast<uint8_t>((t1 >> 6) | ((t2 << 4) & 0xFF)));
    out.SetValue(byteBase + 3U, static_cast<uint8_t>((t2 >> 4) | ((t3 << 6) & 0xFF)));
    out.SetValue(byteBase + 4U, static_cast<uint8_t>(t3 >> 2));
}

__aicore__ inline void poly_byte_encode_d10_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                                  AscendC::LocalTensor<int32_t> &tmp)
{
    mask_low_bits_i32(in, tmp, 10, kPolyLen);
    AscendC::PipeBarrier<PIPE_ALL>();
    for (uint32_t g = 0; g < kPolyLen / 4U; ++g) {
        pack_d10_group(out, in, g);
    }
}

#elif F203_BYTE_ENCODE_D == 11

__aicore__ inline void poly_byte_encode_d11_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                                  AscendC::LocalTensor<int32_t> &)
{
    poly_byte_encode_bits_local(out, in, 11U);
}

#endif

__aicore__ inline void poly_byte_encode_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                              AscendC::LocalTensor<int32_t> &tmp)
{
#if F203_BYTE_ENCODE_D == 4
    poly_byte_encode_d4_local(out, in, tmp);
#elif F203_BYTE_ENCODE_D == 5
    poly_byte_encode_d5_local(out, in, tmp);
#elif F203_BYTE_ENCODE_D == 10
    poly_byte_encode_d10_local(out, in, tmp);
#elif F203_BYTE_ENCODE_D == 11
    poly_byte_encode_d11_local(out, in, tmp);
#endif
}

}  // namespace byte_encode_d

#endif
