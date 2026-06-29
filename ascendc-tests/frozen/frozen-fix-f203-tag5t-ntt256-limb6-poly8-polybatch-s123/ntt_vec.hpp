#ifndef __NTT_VEC_HPP__
#define __NTT_VEC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "stage3_config.hpp"

struct Tensor_int8x4 {
    LocalTensor<int8_t> &x0;
    LocalTensor<int8_t> &x1;
};

template <typename U, typename T>
__aicore__ static inline auto tr(LocalTensor<T> x)
{
    return x.template ReinterpretCast<U>();
}

__aicore__ static inline void split_2xint6(int8_t &d0, int8_t &d1, int32_t a)
{
    d0 = static_cast<int8_t>(a & kKyberLimbMask);
    d1 = static_cast<int8_t>((a >> kKyberLimbBits) & kKyberLimbMask);
}

__aicore__ inline void split_vec(Tensor_int8x4 &dst, LocalTensor<int32_t> &src, const int32_t count)
{
    auto x0 = tr<int32_t>(dst.x0);
    auto x1 = tr<int32_t>(dst.x1);

    for (int32_t i = 0; i < count / 4; i++) {
        int32_t a[4] = {src.GetValue(i * 4 + 0), src.GetValue(i * 4 + 1), src.GetValue(i * 4 + 2),
                        src.GetValue(i * 4 + 3)};

        int8_t d0[4], d1[4];
        for (int j = 0; j < 4; j++) {
            split_2xint6(d0[j], d1[j], a[j]);
        }
        x0.SetValue(i, *(int32_t *)d0);
        x1.SetValue(i, *(int32_t *)d1);
    }
}

__aicore__ inline void wrap_mod_vec_runtime(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &src, int32_t q,
                                            LocalTensor<int32_t> &t1, LocalTensor<int32_t> &t2, int32_t count)
{
    AscendC::Adds(t1, src, -q, count);
    auto &t1_u32 = *reinterpret_cast<LocalTensor<uint32_t> *>(&t1);
    auto &t2_u32 = *reinterpret_cast<LocalTensor<uint32_t> *>(&t2);
    AscendC::ShiftRight(t2_u32, t1_u32, 31U, count);
    AscendC::Mul(t2, src, t2, count);
    AscendC::Max(dst, t1, t2, count);
}

__aicore__ inline void barrett_mul_vec_runtime(LocalTensor<int32_t> &dst, int32_t q, int32_t k, int32_t mu,
                                               LocalTensor<int32_t> &t1, LocalTensor<int32_t> &t2, int32_t count)
{
    AscendC::ShiftRight(t1, dst, static_cast<int32_t>(k - 1), count);
    AscendC::Muls(t1, t1, mu, count);
    AscendC::ShiftRight(t1, t1, static_cast<int32_t>(k + 1), count);
    AscendC::Muls(t1, t1, q, count);
    AscendC::Sub(dst, dst, t1, count);
    wrap_mod_vec_runtime(dst, dst, q, t1, t2, count);
}

/** row[0..2*halfLen) 交错列 → even[halfLen], odd[halfLen]（Gather 索引为字节偏移）。 */
__aicore__ inline void deinterleave_even_odd_vec(LocalTensor<int32_t> &even, LocalTensor<int32_t> &odd,
                                                 LocalTensor<int32_t> &row, LocalTensor<int32_t> &idx,
                                                 LocalTensor<int32_t> &idx2, int32_t halfLen)
{
    using AscendC::Adds;
    using AscendC::CreateVecIndex;
    using AscendC::Gather;
    using AscendC::Muls;
    const uint32_t n = static_cast<uint32_t>(halfLen);
    CreateVecIndex(idx, static_cast<int32_t>(0), n);
    Muls(idx2, idx, static_cast<int32_t>(8), halfLen);
    Gather(even, row, idx2.ReinterpretCast<uint32_t>(), 0U, n);
    Adds(idx2, idx2, static_cast<int32_t>(4), halfLen);
    Gather(odd, row, idx2.ReinterpretCast<uint32_t>(), 0U, n);
}

/** f203_stage3_route_a：raw = hh*4096 + (hl+lh)*64 + ll（合并阶段不取模）。 */
__aicore__ inline void combine_limb6_horner_raw_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                    LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                    LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1, int32_t count)
{
    using AscendC::Add;
    using AscendC::ShiftLeft;
    ShiftLeft(dst, hh, kKyberMergeShift1, count);
    Add(t1, hl, lh, count);
    Add(dst, dst, t1, count);
    ShiftLeft(dst, dst, kKyberMergeShift1, count);
    Add(dst, dst, ll, count);
}

#include "stage3_mod_variants.hpp"

#if F203_STAGE3_MOD == 0
__aicore__ inline void combine_limb6_routea_mod_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                    LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                    LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                    LocalTensor<int32_t> &t2, int32_t q, int32_t count)
{
    combine_limb6_horner_barrett_vec(dst, hh, lh, hl, ll, t1, t2, q, count);
}
#elif F203_STAGE3_MOD == 1
__aicore__ inline void combine_limb6_routea_mod_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                    LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                    LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                    LocalTensor<int32_t> &t2, int32_t q, int32_t count)
{
    (void)t2;
    combine_limb6_routea_mod_scalar_i64(dst, hh, lh, hl, ll, t1, q, count);
}
#else
__aicore__ inline void combine_limb6_routea_mod_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                    LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                    LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                    LocalTensor<float> &fRaw, LocalTensor<float> &fTmp,
                                                    LocalTensor<float> &fQuot, int32_t q, int32_t count)
{
    combine_limb6_routea_mod_cast_div(dst, hh, lh, hl, ll, t1, fRaw, fTmp, fQuot, q, count);
}
#endif

#endif
