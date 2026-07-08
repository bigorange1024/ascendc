#ifndef BYTE_DECODE_D_VEC_HPP
#define BYTE_DECODE_D_VEC_HPP

#include "byte_decode_d_config.hpp"
#include "f203_mlkem_params.h"
#include "kernel_operator.h"

namespace byte_decode_d {

constexpr uint32_t kPolyLen = static_cast<uint32_t>(F203_MLKEM_N);
constexpr uint32_t kInBytes = static_cast<uint32_t>(F203_BYTE_DECODE_POLY_BYTES);

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

__aicore__ inline void widen_bytes_to_i32(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                          uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        out.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(in.GetValue(i)));
    }
}

#if F203_BYTE_DECODE_D == 4

__aicore__ inline void unpack_d4_pair(AscendC::LocalTensor<int32_t> &out, int32_t byteVal, uint32_t pairIdx)
{
    const int32_t lo = byteVal & 0xF;
    const int32_t hi = (byteVal >> 4) & 0xF;
    out.SetValue(static_cast<int32_t>(2U * pairIdx), lo);
    out.SetValue(static_cast<int32_t>(2U * pairIdx + 1U), hi);
}

#if BYTE_DECODE_D_VEC >= 1

__aicore__ inline void poly_byte_decode_d4_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                 AscendC::LocalTensor<int32_t> &tmp,
                                                 AscendC::LocalTensor<int32_t> &hi)
{
    constexpr uint32_t kPairs = kPolyLen / 2U;
    widen_bytes_to_i32(tmp, in, kPairs);
    AscendC::PipeBarrier<PIPE_ALL>();
    mask_low_bits_i32(tmp, hi, 4, kPairs);
    AscendC::PipeBarrier<PIPE_ALL>();

    for (uint32_t i = 0; i < kPairs; ++i) {
        const int32_t b = static_cast<int32_t>(in.GetValue(i));
        out.SetValue(static_cast<int32_t>(2U * i), tmp.GetValue(static_cast<int32_t>(i)));
        out.SetValue(static_cast<int32_t>(2U * i + 1U), (b >> 4) & 0xF);
    }
}

#else

__aicore__ inline void poly_byte_decode_d4_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                 AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t i = 0; i < kPolyLen / 2U; ++i) {
        unpack_d4_pair(out, static_cast<int32_t>(in.GetValue(i)), i);
    }
}

#endif

#elif F203_BYTE_DECODE_D == 5

/** 5B/组 → 8×5bit 系数（Alg.6 逆，与 encode d=5 对称）。 */
__aicore__ inline void unpack_d5_group(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                        uint32_t group)
{
    const uint32_t byteBase = group * 5U;
    const uint8_t b0 = in.GetValue(byteBase + 0U);
    const uint8_t b1 = in.GetValue(byteBase + 1U);
    const uint8_t b2 = in.GetValue(byteBase + 2U);
    const uint8_t b3 = in.GetValue(byteBase + 3U);
    const uint8_t b4 = in.GetValue(byteBase + 4U);
    const uint32_t coeffBase = group * 8U;
    out.SetValue(static_cast<int32_t>(coeffBase + 0U), static_cast<int32_t>(0x1Fu & (b0 >> 0)));
    out.SetValue(static_cast<int32_t>(coeffBase + 1U), static_cast<int32_t>(0x1Fu & ((b0 >> 5) | (b1 << 3))));
    out.SetValue(static_cast<int32_t>(coeffBase + 2U), static_cast<int32_t>(0x1Fu & (b1 >> 2)));
    out.SetValue(static_cast<int32_t>(coeffBase + 3U), static_cast<int32_t>(0x1Fu & ((b1 >> 7) | (b2 << 1))));
    out.SetValue(static_cast<int32_t>(coeffBase + 4U), static_cast<int32_t>(0x1Fu & ((b2 >> 4) | (b3 << 4))));
    out.SetValue(static_cast<int32_t>(coeffBase + 5U), static_cast<int32_t>(0x1Fu & (b3 >> 1)));
    out.SetValue(static_cast<int32_t>(coeffBase + 6U), static_cast<int32_t>(0x1Fu & ((b3 >> 6) | (b4 << 2))));
    out.SetValue(static_cast<int32_t>(coeffBase + 7U), static_cast<int32_t>(0x1Fu & (b4 >> 3)));
}

#if BYTE_DECODE_D_VEC >= 1

__aicore__ inline void poly_byte_decode_d5_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                 AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        unpack_d5_group(out, in, g);
    }
}

#else

__aicore__ inline void poly_byte_decode_d5_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                 AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        unpack_d5_group(out, in, g);
    }
}

#endif

#elif F203_BYTE_DECODE_D == 10

__aicore__ inline void unpack_d10_group(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                        uint32_t group)
{
    const uint32_t byteBase = group * 5U;
    const uint8_t b0 = in.GetValue(byteBase + 0U);
    const uint8_t b1 = in.GetValue(byteBase + 1U);
    const uint8_t b2 = in.GetValue(byteBase + 2U);
    const uint8_t b3 = in.GetValue(byteBase + 3U);
    const uint8_t b4 = in.GetValue(byteBase + 4U);
    const uint16_t t0 = static_cast<uint16_t>(0x3FFu & ((b0 >> 0) | ((uint16_t)b1 << 8)));
    const uint16_t t1 = static_cast<uint16_t>(0x3FFu & ((b1 >> 2) | ((uint16_t)b2 << 6)));
    const uint16_t t2 = static_cast<uint16_t>(0x3FFu & ((b2 >> 4) | ((uint16_t)b3 << 4)));
    const uint16_t t3 = static_cast<uint16_t>(0x3FFu & ((b3 >> 6) | ((uint16_t)b4 << 2)));
    const uint32_t coeffBase = group * 4U;
    out.SetValue(static_cast<int32_t>(coeffBase + 0U), static_cast<int32_t>(t0));
    out.SetValue(static_cast<int32_t>(coeffBase + 1U), static_cast<int32_t>(t1));
    out.SetValue(static_cast<int32_t>(coeffBase + 2U), static_cast<int32_t>(t2));
    out.SetValue(static_cast<int32_t>(coeffBase + 3U), static_cast<int32_t>(t3));
}

#if BYTE_DECODE_D_VEC >= 1

__aicore__ inline void poly_byte_decode_d10_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                  AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 4U; ++g) {
        unpack_d10_group(out, in, g);
    }
}

#else

__aicore__ inline void poly_byte_decode_d10_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                  AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 4U; ++g) {
        unpack_d10_group(out, in, g);
    }
}

#endif

#elif F203_BYTE_DECODE_D == 11

/** 11B/组 → 8×11bit 系数（ML-KEM-1024 c₁）。 */
__aicore__ inline void unpack_d11_group(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                        uint32_t group)
{
    const uint32_t byteBase = group * 11U;
    const uint8_t b0 = in.GetValue(byteBase + 0U);
    const uint8_t b1 = in.GetValue(byteBase + 1U);
    const uint8_t b2 = in.GetValue(byteBase + 2U);
    const uint8_t b3 = in.GetValue(byteBase + 3U);
    const uint8_t b4 = in.GetValue(byteBase + 4U);
    const uint8_t b5 = in.GetValue(byteBase + 5U);
    const uint8_t b6 = in.GetValue(byteBase + 6U);
    const uint8_t b7 = in.GetValue(byteBase + 7U);
    const uint8_t b8 = in.GetValue(byteBase + 8U);
    const uint8_t b9 = in.GetValue(byteBase + 9U);
    const uint8_t b10 = in.GetValue(byteBase + 10U);
    const uint16_t t0 = static_cast<uint16_t>(0x7FFu & ((b0 >> 0) | ((uint16_t)b1 << 8)));
    const uint16_t t1 = static_cast<uint16_t>(0x7FFu & ((b1 >> 3) | ((uint16_t)b2 << 5)));
    const uint16_t t2 = static_cast<uint16_t>(0x7FFu & ((b2 >> 6) | ((uint16_t)b3 << 2) | ((uint16_t)b4 << 10)));
    const uint16_t t3 = static_cast<uint16_t>(0x7FFu & ((b4 >> 1) | ((uint16_t)b5 << 7)));
    const uint16_t t4 = static_cast<uint16_t>(0x7FFu & ((b5 >> 4) | ((uint16_t)b6 << 4)));
    const uint16_t t5 = static_cast<uint16_t>(0x7FFu & ((b6 >> 7) | ((uint16_t)b7 << 1) | ((uint16_t)b8 << 9)));
    const uint16_t t6 = static_cast<uint16_t>(0x7FFu & ((b8 >> 2) | ((uint16_t)b9 << 6)));
    const uint16_t t7 = static_cast<uint16_t>(0x7FFu & ((b9 >> 5) | ((uint16_t)b10 << 3)));
    const uint32_t coeffBase = group * 8U;
    out.SetValue(static_cast<int32_t>(coeffBase + 0U), static_cast<int32_t>(t0));
    out.SetValue(static_cast<int32_t>(coeffBase + 1U), static_cast<int32_t>(t1));
    out.SetValue(static_cast<int32_t>(coeffBase + 2U), static_cast<int32_t>(t2));
    out.SetValue(static_cast<int32_t>(coeffBase + 3U), static_cast<int32_t>(t3));
    out.SetValue(static_cast<int32_t>(coeffBase + 4U), static_cast<int32_t>(t4));
    out.SetValue(static_cast<int32_t>(coeffBase + 5U), static_cast<int32_t>(t5));
    out.SetValue(static_cast<int32_t>(coeffBase + 6U), static_cast<int32_t>(t6));
    out.SetValue(static_cast<int32_t>(coeffBase + 7U), static_cast<int32_t>(t7));
}

#if BYTE_DECODE_D_VEC >= 1

__aicore__ inline void poly_byte_decode_d11_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                  AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        unpack_d11_group(out, in, g);
    }
}

#else

__aicore__ inline void poly_byte_decode_d11_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                  AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        unpack_d11_group(out, in, g);
    }
}

#endif

#endif

__aicore__ inline void poly_byte_decode_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                              AscendC::LocalTensor<int32_t> &tmp, AscendC::LocalTensor<int32_t> &hi)
{
#if F203_BYTE_DECODE_D == 4
    poly_byte_decode_d4_local(out, in, tmp, hi);
#elif F203_BYTE_DECODE_D == 5
    poly_byte_decode_d5_local(out, in, tmp, hi);
#elif F203_BYTE_DECODE_D == 10
    poly_byte_decode_d10_local(out, in, tmp, hi);
#elif F203_BYTE_DECODE_D == 11
    poly_byte_decode_d11_local(out, in, tmp, hi);
#endif
}

}  // namespace byte_decode_d

#endif
