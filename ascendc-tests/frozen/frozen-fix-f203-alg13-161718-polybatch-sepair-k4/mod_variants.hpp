#ifndef F203_MOD_VARIANTS_HPP
#define F203_MOD_VARIANTS_HPP

#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "mod_config.hpp"

/** Barrett 参数：q=3329，见 ntt_study mlkem_ntt_tables.h / merged_kyber */
constexpr int32_t kF203BarrettMu = 314;
constexpr int32_t kF203BarrettK = 20;

// ---------------------------------------------------------------------------
// 变体 0：标量 int64 floor %
// ---------------------------------------------------------------------------

__aicore__ inline int32_t mod_q_scalar_i64_one(int64_t x, int32_t q)
{
    const int64_t q64 = static_cast<int64_t>(q);
    const int64_t t = (x >= 0) ? (x / q64) : (-((-x) / q64));
    int64_t rem = x - q64 * t;
    if (rem >= q64) {
        rem -= q64;
    }
    if (rem < 0) {
        rem += q64;
    }
    return static_cast<int32_t>(rem);
}

__aicore__ inline void mod_q_scalar_i64_vec(LocalTensor<int32_t> &dst, int32_t q, int32_t count)
{
    for (int32_t i = 0; i < count; ++i) {
        const int64_t raw = static_cast<int64_t>(dst.GetValue(i));
        dst.SetValue(i, mod_q_scalar_i64_one(raw, q));
    }
}

// ---------------------------------------------------------------------------
// 变体 1：Barrett 向量约化（merged_kyber / limb6 Stage3）
// ---------------------------------------------------------------------------

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

__aicore__ inline void mod_q_barrett_vec(LocalTensor<int32_t> &dst, int32_t q, LocalTensor<int32_t> &t1,
                                         LocalTensor<int32_t> &t2, int32_t count)
{
    AscendC::Muls(t1, dst, kF203BarrettMu, count);
    AscendC::ShiftRight(t1, t1, kF203BarrettK, count);
    AscendC::Muls(t2, t1, q, count);
    AscendC::Sub(dst, dst, t2, count);
    wrap_mod_vec_runtime(dst, dst, q, t1, t2, count);
}

/** acc=hh; acc=acc*64+(hl+lh); acc=acc*64+ll，每步 Barrett。 */
__aicore__ inline void combine_limb6_horner_barrett_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                        LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                        LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                        LocalTensor<int32_t> &t2, int32_t q, int32_t count)
{
    using AscendC::Add;
    using AscendC::DataCopy;
    using AscendC::ShiftLeft;
    DataCopy(dst, hh, static_cast<uint32_t>(count));
    mod_q_barrett_vec(dst, q, t1, t2, count);
    Add(t1, hl, lh, count);
    ShiftLeft(dst, dst, kKyberMergeShift1, count);
    Add(dst, dst, t1, count);
    mod_q_barrett_vec(dst, q, t1, t2, count);
    ShiftLeft(dst, dst, kKyberMergeShift1, count);
    Add(dst, dst, ll, count);
    mod_q_barrett_vec(dst, q, t1, t2, count);
}

// ---------------------------------------------------------------------------
// 变体 2：Cast→float Div→Muls/Sub 向量约化（ntt_study / ONNX Stage3.1）
// ---------------------------------------------------------------------------

__aicore__ inline void mod_q_cast_div_vec(LocalTensor<int32_t> &dst, int32_t q, LocalTensor<int32_t> &t1,
                                          LocalTensor<float> &fRaw, LocalTensor<float> &fTmp,
                                          LocalTensor<float> &fQuot, int32_t count)
{
    using AscendC::Cast;
    using AscendC::Div;
    using AscendC::Duplicate;
    using AscendC::Muls;
    using AscendC::Sub;
    const uint32_t n = static_cast<uint32_t>(count);

    Cast(fRaw, dst, AscendC::RoundMode::CAST_NONE, n);
    Duplicate(t1, q, count);
    Cast(fTmp, t1, AscendC::RoundMode::CAST_NONE, n);
    Div(fQuot, fRaw, fTmp, count);
    Cast(t1, fQuot, AscendC::RoundMode::CAST_TRUNC, n);
    Muls(t1, t1, q, count);
    Sub(dst, dst, t1, count);
}

// ---------------------------------------------------------------------------
// 统一入口宏：变体 0/1 用 MOD_Q_I32；变体 2 用 MOD_Q_CAST
// ---------------------------------------------------------------------------

#if F203_MOD_VARIANT == 0
#define MOD_Q_I32(DST, Q, T1, T2, N) mod_q_scalar_i64_vec((DST), (Q), (N))
#elif F203_MOD_VARIANT == 1
#define MOD_Q_I32(DST, Q, T1, T2, N) mod_q_barrett_vec((DST), (Q), (T1), (T2), (N))
#endif

#if F203_MOD_VARIANT == 2
#define MOD_Q_CAST(DST, Q, T1, FR, FT, FQ, N) mod_q_cast_div_vec((DST), (Q), (T1), (FR), (FT), (FQ), (N))
#endif

#endif
