/**
 * @file compress_d_vec.hpp
 * @brief FIPS 203 Compress_d 设备实现（d=4/5/10/11）。
 *
 * COMPRESS_D_VEC=1（默认）：per-lane 向量（Barrett d=4/5 或 cast_div 商 d=10/11）；Encrypt tail 抄此路径。
 * COMPRESS_D_VEC=0：标量 fallback，仅对照。
 * 与 ByteEncode 不同：无 bit shuffle → 默认**激活**向量。见 docs/notes/F203-Compress-Decompress-向量实现指南.md。
 */
#ifndef COMPRESS_D_VEC_HPP
#define COMPRESS_D_VEC_HPP

#include "compress_d_config.hpp"
#include "f203_compress_d_params.hpp"
#include "f203_mlkem_params.h"
#include "kernel_operator.h"

namespace compress_d {

constexpr uint32_t kPolyLen = static_cast<uint32_t>(F203_MLKEM_N);

__aicore__ inline uint32_t scalar_compress_u32(uint32_t u)
{
#if F203_COMPRESS_D == 4
    const uint32_t d0 = u * 1290160u;
    return (d0 + (1u << 27)) >> 28;
#elif F203_COMPRESS_D == 5
    const uint32_t d0 = u * 1290176u;
    return ((d0 + (1u << 26)) >> 27) & 0x1fu;
#elif F203_COMPRESS_D == 10
    uint64_t d0 = static_cast<uint64_t>(u) * 2642263040ull;
    d0 = (d0 + (static_cast<uint64_t>(1) << 32)) >> 33;
    return static_cast<uint32_t>(d0 & 0x3ffu);
#elif F203_COMPRESS_D == 11
    uint64_t d0 = static_cast<uint64_t>(u) * 5284526080ull;
    d0 = (d0 + (static_cast<uint64_t>(1) << 32)) >> 33;
    return static_cast<uint32_t>(d0 & 0x7ffu);
#endif
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

#if !F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1

/** int32 Barrett 向量：d=4/5（magic 乘积可放进 u32 lane）。 */
__aicore__ inline void poly_compress_barrett_vec(AscendC::LocalTensor<int32_t> &out,
                                                 AscendC::LocalTensor<int32_t> &in,
                                                 AscendC::LocalTensor<int32_t> &tmp)
{
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    const int32_t n = static_cast<int32_t>(kPolyLen);
    Muls(out, in, static_cast<int32_t>(F203_COMPRESS_BARRETT_MUL), n);
    Adds(out, out, static_cast<int32_t>(F203_COMPRESS_BARRETT_BIAS), n);
    ShiftRight(out, out, F203_COMPRESS_BARRETT_SHIFT, n);
    mask_low_bits_i32(out, tmp, F203_COMPRESS_D_BITS, kPolyLen);
}

#endif

#if F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1

/**
 * cast_div 商向量：d=10/11。
 * round(u·2^d/q) = floor((u·2^d + q/2)/q)；与 liboqs Barrett 标量全 u∈[0,q) 0 差异。
 */
__aicore__ inline void poly_compress_cast_div_vec(AscendC::LocalTensor<int32_t> &out,
                                                  AscendC::LocalTensor<int32_t> &in,
                                                  AscendC::LocalTensor<int32_t> &tmp_i,
                                                  AscendC::LocalTensor<float> &fRaw,
                                                  AscendC::LocalTensor<float> &fTmp,
                                                  AscendC::LocalTensor<float> &fQuot)
{
    using AscendC::Adds;
    using AscendC::Cast;
    using AscendC::Div;
    using AscendC::Duplicate;
    using AscendC::Muls;
    const int32_t n = static_cast<int32_t>(kPolyLen);
    const int32_t kScale = static_cast<int32_t>(1) << F203_COMPRESS_D_BITS;
    const int32_t kRoundBias = static_cast<int32_t>(F203_MLKEM_Q / 2);

    Muls(tmp_i, in, kScale, n);
    Adds(tmp_i, tmp_i, kRoundBias, n);
    Cast(fRaw, tmp_i, AscendC::RoundMode::CAST_NONE, static_cast<uint32_t>(n));
    Duplicate(tmp_i, static_cast<int32_t>(F203_MLKEM_Q), n);
    Cast(fTmp, tmp_i, AscendC::RoundMode::CAST_NONE, static_cast<uint32_t>(n));
    Div(fQuot, fRaw, fTmp, n);
    Cast(out, fQuot, AscendC::RoundMode::CAST_TRUNC, static_cast<uint32_t>(n));
    mask_low_bits_i32(out, tmp_i, F203_COMPRESS_D_BITS, kPolyLen);
}

#endif

__aicore__ inline void poly_compress_scalar(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in)
{
    for (uint32_t i = 0; i < kPolyLen; ++i) {
        const uint32_t u = static_cast<uint32_t>(in.GetValue(static_cast<int32_t>(i)));
        out.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(scalar_compress_u32(u)));
    }
}

__aicore__ inline void poly_compress_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                           AscendC::LocalTensor<int32_t> &tmp)
{
#if !F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1
    poly_compress_barrett_vec(out, in, tmp);
#else
    (void)tmp;
    poly_compress_scalar(out, in);
#endif
}

#if F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1
__aicore__ inline void poly_compress_cast_div_dispatch(AscendC::LocalTensor<int32_t> &out,
                                                       AscendC::LocalTensor<int32_t> &in,
                                                       AscendC::LocalTensor<int32_t> &tmp_i,
                                                       AscendC::LocalTensor<float> &fRaw,
                                                       AscendC::LocalTensor<float> &fTmp,
                                                       AscendC::LocalTensor<float> &fQuot)
{
    poly_compress_cast_div_vec(out, in, tmp_i, fRaw, fTmp, fQuot);
}
#endif

} // namespace compress_d

#endif
