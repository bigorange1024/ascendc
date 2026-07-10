/**
 * @file f203_unified_compress_vec.hpp
 * @brief FIPS 203 Compress_d — 统一整数舍入设备实现（全档 d 共用 C=41285357）。
 *
 * COMPRESS_UNIFIED_INT_VEC=1（默认）：纯 int32 向量 limb 宽乘（C=C1·2^16+C0；进位安全合并）。
 * COMPRESS_UNIFIED_INT_VEC=0：逐 lane int64 标量对照。
 */
#ifndef F203_UNIFIED_COMPRESS_VEC_HPP
#define F203_UNIFIED_COMPRESS_VEC_HPP

#include "f203_unified_round_params.hpp"
#include "kernel_operator.h"

#ifndef F203_MLKEM_N
#error "include f203_mlkem_params.h before f203_unified_compress_vec.hpp"
#endif

namespace f203_unified_round {

constexpr uint32_t kPolyLen = static_cast<uint32_t>(F203_MLKEM_N);

/** v ← v mod 2^bits（移位+乘+减；lo 为正时等价于 lo & (2^bits-1)）。 */
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

__aicore__ inline int32_t compress_unified_one(int32_t u)
{
    const int64_t prod = static_cast<int64_t>(F203_UNIFIED_ROUND_C) * static_cast<int64_t>(u);
    const int64_t s = prod + static_cast<int64_t>(F203_UNIFIED_COMPRESS_BIAS);
    return static_cast<int32_t>(s >> F203_UNIFIED_COMPRESS_SHIFT);
}

#if COMPRESS_UNIFIED_INT_VEC >= 1

/**
 * 纯 int32 向量统一 Compress（limb 宽乘）。
 *
 * carry = (lo>>16) + bias_hi + ((lo mod 2^16) + bias_lo) >> 16
 * out   = (hi + carry) >> (k - 16)
 *
 * @param scratch 长度 kPolyLen 的独立 UB 缓冲，供 mask_low_bits 与 carry_i 使用（勿与 out/hi/carry 别名）。
 */
__aicore__ inline void poly_compress_unified_vec(AscendC::LocalTensor<int32_t> &out,
                                                   AscendC::LocalTensor<int32_t> &in,
                                                   AscendC::LocalTensor<int32_t> &lo,
                                                   AscendC::LocalTensor<int32_t> &hi,
                                                   AscendC::LocalTensor<int32_t> &carry,
                                                   AscendC::LocalTensor<int32_t> &scratch)
{
    using AscendC::Add;
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    const int32_t n = static_cast<int32_t>(kPolyLen);

    Muls(lo, in, static_cast<int32_t>(F203_UNIFIED_COMPRESS_C0), n);
    Muls(hi, in, static_cast<int32_t>(F203_UNIFIED_COMPRESS_C1), n);

    ShiftRight(carry, lo, F203_UNIFIED_COMPRESS_LIMB_SHIFT, n);
    mask_low_bits_i32(lo, scratch, F203_UNIFIED_COMPRESS_LIMB_SHIFT, kPolyLen);
    Adds(lo, lo, static_cast<int32_t>(F203_UNIFIED_COMPRESS_BIAS_LO), n);
    ShiftRight(scratch, lo, F203_UNIFIED_COMPRESS_LIMB_SHIFT, n);
    Adds(carry, carry, static_cast<int32_t>(F203_UNIFIED_COMPRESS_BIAS_HI), n);
    Add(carry, carry, scratch, n);
    Add(hi, hi, carry, n);
    ShiftRight(out, hi, F203_UNIFIED_COMPRESS_ACC_SHIFT, n);
}

#endif

__aicore__ inline void poly_compress_unified_local(AscendC::LocalTensor<int32_t> &out,
                                                   AscendC::LocalTensor<int32_t> &in,
                                                   AscendC::LocalTensor<int32_t> &lo,
                                                   AscendC::LocalTensor<int32_t> &hi,
                                                   AscendC::LocalTensor<int32_t> &carry,
                                                   AscendC::LocalTensor<int32_t> &scratch)
{
#if COMPRESS_UNIFIED_INT_VEC >= 1
    poly_compress_unified_vec(out, in, lo, hi, carry, scratch);
#else
    (void)lo;
    (void)hi;
    (void)carry;
    (void)scratch;
    for (uint32_t i = 0; i < kPolyLen; ++i) {
        const int32_t x = in.GetValue(static_cast<int32_t>(i));
        out.SetValue(static_cast<int32_t>(i), compress_unified_one(x));
    }
#endif
}

} // namespace f203_unified_round

#endif
