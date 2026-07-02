#ifndef COMPRESS_D_VEC_HPP
#define COMPRESS_D_VEC_HPP

#include "compress_d_config.hpp"
#include "f203_mlkem_params.h"
#include "kernel_operator.h"

namespace compress_d {

constexpr uint32_t kPolyLen = static_cast<uint32_t>(F203_MLKEM_N);
constexpr uint32_t kTileLen = 128U;

__aicore__ inline uint32_t scalar_compress_d4_u32(uint32_t u)
{
    uint32_t d0 = u * 1290160u;
    return (d0 + (1u << 27)) >> 28;
}

__aicore__ inline uint32_t scalar_compress_d5_u32(uint32_t u)
{
    uint32_t d0 = u * 1290176u;
    return ((d0 + (1u << 26)) >> 27) & 0x1fu;
}

__aicore__ inline uint32_t scalar_compress_d10_u32(uint32_t u)
{
    uint64_t d0 = static_cast<uint64_t>(u) * 2642263040ull;
    d0 = (d0 + (static_cast<uint64_t>(1) << 32)) >> 33;
    return static_cast<uint32_t>(d0 & 0x3ffu);
}

__aicore__ inline uint32_t scalar_compress_d11_u32(uint32_t u)
{
    uint64_t d0 = static_cast<uint64_t>(u) * 5284526080ull;
    d0 = (d0 + (static_cast<uint64_t>(1) << 32)) >> 33;
    return static_cast<uint32_t>(d0 & 0x7ffu);
}

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

#if F203_COMPRESS_D == 4 && COMPRESS_D_VEC >= 1

__aicore__ inline void compress_d4_vec_tile(AscendC::LocalTensor<int32_t> &out,
                                            AscendC::LocalTensor<int32_t> &in,
                                            AscendC::LocalTensor<int32_t> &tmp, uint32_t count)
{
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    const int32_t n = static_cast<int32_t>(count);
    Muls(out, in, static_cast<int32_t>(1290160), n);
    Adds(out, out, static_cast<int32_t>(1 << 27), n);
    ShiftRight(out, out, 28, n);
    mask_low_bits_i32(out, tmp, 4, count);
}

__aicore__ inline void poly_compress_d4_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                              AscendC::LocalTensor<int32_t> &tmp)
{
    compress_d4_vec_tile(out, in, tmp, kPolyLen);
}

#else

__aicore__ inline void poly_compress_d4_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                              AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t i = 0; i < kPolyLen; ++i) {
        const uint32_t u = static_cast<uint32_t>(in.GetValue(static_cast<int32_t>(i)));
        out.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(scalar_compress_d4_u32(u)));
    }
}

#endif

#if F203_COMPRESS_D == 5

__aicore__ inline void poly_compress_d5_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                              AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t i = 0; i < kPolyLen; ++i) {
        const uint32_t u = static_cast<uint32_t>(in.GetValue(static_cast<int32_t>(i)));
        out.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(scalar_compress_d5_u32(u)));
    }
}

#endif

#if F203_COMPRESS_D == 10

__aicore__ inline void poly_compress_d10_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                               AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t i = 0; i < kPolyLen; ++i) {
        const uint32_t u = static_cast<uint32_t>(in.GetValue(static_cast<int32_t>(i)));
        out.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(scalar_compress_d10_u32(u)));
    }
}

#endif

#if F203_COMPRESS_D == 11

__aicore__ inline void poly_compress_d11_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                               AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t i = 0; i < kPolyLen; ++i) {
        const uint32_t u = static_cast<uint32_t>(in.GetValue(static_cast<int32_t>(i)));
        out.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(scalar_compress_d11_u32(u)));
    }
}

#endif

__aicore__ inline void poly_compress_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                           AscendC::LocalTensor<int32_t> &tmp)
{
#if F203_COMPRESS_D == 4
    poly_compress_d4_local(out, in, tmp);
#elif F203_COMPRESS_D == 5
    poly_compress_d5_local(out, in, tmp);
#elif F203_COMPRESS_D == 10
    poly_compress_d10_local(out, in, tmp);
#elif F203_COMPRESS_D == 11
    poly_compress_d11_local(out, in, tmp);
#endif
}

}  // namespace compress_d

#endif
