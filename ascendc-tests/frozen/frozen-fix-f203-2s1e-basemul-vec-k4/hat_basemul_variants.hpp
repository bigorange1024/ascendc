#ifndef HAT_BASEMUL_VARIANTS_HPP
#define HAT_BASEMUL_VARIANTS_HPP

#include "basemul_config.hpp"
#include "basic.hpp"
#include "hat_gammas.hpp"
#include "hat_vec.hpp"
#include "kernel_operator.h"

#if HAT_BASEMUL_VARIANT >= 1

namespace hat_basemul {

struct BasemulWs {
    LocalTensor<int32_t> a0;
    LocalTensor<int32_t> a1;
    LocalTensor<int32_t> b0;
    LocalTensor<int32_t> b1;
    LocalTensor<int32_t> c0;
    LocalTensor<int32_t> c1;
    LocalTensor<int32_t> t1;
    LocalTensor<int32_t> t2;
    LocalTensor<int32_t> gammaV;
    LocalTensor<int32_t> idx;
    LocalTensor<int32_t> idx2;
};

__aicore__ inline void bind_basemul_ws(LocalTensor<int32_t> &base, BasemulWs &w, uint32_t pairCount)
{
    (void)pairCount;
    w.a0 = base[0];
    w.a1 = base[pairCount];
    w.b0 = base[2U * pairCount];
    w.b1 = base[3U * pairCount];
    w.c0 = base[4U * pairCount];
    w.c1 = base[5U * pairCount];
    w.t1 = base[6U * pairCount];
    w.t2 = base[7U * pairCount];
    w.gammaV = base[8U * pairCount];
    w.idx = base[9U * pairCount];
    w.idx2 = base[10U * pairCount];
}

__aicore__ inline void deinterleave_pairs_scalar(LocalTensor<int32_t> &a0, LocalTensor<int32_t> &a1,
                                                 LocalTensor<int32_t> &f, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        a0.SetValue(i, f.GetValue(i * 2));
        a1.SetValue(i, f.GetValue(i * 2 + 1));
    }
}

__aicore__ inline void interleave_pairs_scalar(LocalTensor<int32_t> &h, LocalTensor<int32_t> &c0,
                                               LocalTensor<int32_t> &c1, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        h.SetValue(i * 2, c0.GetValue(i));
        h.SetValue(i * 2 + 1, c1.GetValue(i));
    }
}

__aicore__ inline void gamma_broadcast(LocalTensor<int32_t> &gammaV, int32_t gammaOff, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        gammaV.SetValue(i, kHatGammas[gammaOff + i]);
    }
}

/** 向量 Barrett；SIM 上纯向量约化曾致 t_hat 溢出，final clamp 仍标量。 */
__aicore__ inline void hat_reduce_zq_vec_barrett(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &tmp, int32_t count)
{
    using AscendC::Muls;
    using AscendC::ShiftRight;
    using AscendC::Sub;

    const int32_t n = count;
    const int32_t q = kHatQ;

    Muls(tmp, dst, 78, n);
    ShiftRight(tmp, tmp, 18, n);
    Muls(tmp, tmp, q, n);
    Sub(dst, dst, tmp, n);
    KYBER_PIPE_ALL();

    Muls(tmp, dst, 5039, n);
    ShiftRight(tmp, tmp, 24, n);
    Muls(tmp, tmp, q, n);
    Sub(dst, dst, tmp, n);
    KYBER_PIPE_ALL();

    for (int32_t i = 0; i < n; ++i) {
        dst.SetValue(i, hat_reduce_zq_scalar(dst.GetValue(i)));
    }
}

__aicore__ inline void coef_pairs_gather(LocalTensor<int32_t> &a0, LocalTensor<int32_t> &a1, LocalTensor<int32_t> &f,
                                         LocalTensor<int32_t> &idx, LocalTensor<int32_t> &idx2, int32_t pairCount)
{
    using AscendC::Adds;
    using AscendC::CreateVecIndex;
    using AscendC::Gather;
    using AscendC::Muls;

    const uint32_t n = static_cast<uint32_t>(pairCount);
    CreateVecIndex(idx, static_cast<int32_t>(0), n);
    Muls(idx2, idx, static_cast<int32_t>(8), pairCount);
    Gather(a0, f, idx2.ReinterpretCast<uint32_t>(), 0U, n);
    Adds(idx2, idx2, static_cast<int32_t>(4), pairCount);
    Gather(a1, f, idx2.ReinterpretCast<uint32_t>(), 0U, n);
}

/** B1：无 Gather，标量 deinterleave + 向量 basemul。 */
__aicore__ inline void multiply_ntts_half_deinterleave_vec(LocalTensor<int32_t> &h, LocalTensor<int32_t> &f,
                                                           LocalTensor<int32_t> &g, BasemulWs &w, int32_t pairCount,
                                                           int32_t gammaOff)
{
    using AscendC::Add;
    using AscendC::Mul;

    const int32_t n = pairCount;

    deinterleave_pairs_scalar(w.a0, w.a1, f, n);
    deinterleave_pairs_scalar(w.b0, w.b1, g, n);
    gamma_broadcast(w.gammaV, gammaOff, n);
    KYBER_PIPE_ALL();

    Mul(w.t1, w.a1, w.b1, n);
    hat_reduce_zq_vec_barrett(w.t1, w.t2, n);
    Mul(w.t2, w.t1, w.gammaV, n);
    Mul(w.t1, w.a0, w.b0, n);
    Add(w.c0, w.t1, w.t2, n);
    hat_reduce_zq_vec_barrett(w.c0, w.t2, n);
    KYBER_PIPE_ALL();

    Mul(w.t1, w.a0, w.b1, n);
    Mul(w.t2, w.a1, w.b0, n);
    Add(w.c1, w.t1, w.t2, n);
    hat_reduce_zq_vec_barrett(w.c1, w.t2, n);
    KYBER_PIPE_ALL();

    interleave_pairs_scalar(h, w.c0, w.c1, n);
    KYBER_PIPE_ALL();
}

/** B2：Gather deinterleave + 向量 basemul（全向量 Mul/Add/reduce，γ 向量广播）。 */
__aicore__ inline void multiply_ntts_half_gather_vec(LocalTensor<int32_t> &h, LocalTensor<int32_t> &f,
                                                     LocalTensor<int32_t> &g, BasemulWs &w, int32_t pairCount,
                                                     int32_t gammaOff)
{
    using AscendC::Add;
    using AscendC::Mul;

    const int32_t n = pairCount;

    coef_pairs_gather(w.a0, w.a1, f, w.idx, w.idx2, n);
    coef_pairs_gather(w.b0, w.b1, g, w.idx, w.idx2, n);
    gamma_broadcast(w.gammaV, gammaOff, n);
    KYBER_PIPE_ALL();

    Mul(w.t1, w.a1, w.b1, n);
    hat_reduce_zq_vec_barrett(w.t1, w.t2, n);
    Mul(w.t2, w.t1, w.gammaV, n);
    Mul(w.t1, w.a0, w.b0, n);
    Add(w.c0, w.t1, w.t2, n);
    hat_reduce_zq_vec_barrett(w.c0, w.t2, n);
    KYBER_PIPE_ALL();

    Mul(w.t1, w.a0, w.b1, n);
    Mul(w.t2, w.a1, w.b0, n);
    Add(w.c1, w.t1, w.t2, n);
    hat_reduce_zq_vec_barrett(w.c1, w.t2, n);
    KYBER_PIPE_ALL();

    interleave_pairs_scalar(h, w.c0, w.c1, n);
    KYBER_PIPE_ALL();
}

} // namespace hat_basemul

#endif // HAT_BASEMUL_VARIANT >= 1

#if HAT_BASEMUL_VARIANT == 0
__aicore__ inline void multiply_ntts_half_dispatch(LocalTensor<int32_t> &h, LocalTensor<int32_t> &f,
                                                   LocalTensor<int32_t> &g, int32_t pairCount, int32_t gammaOff)
{
    multiply_ntts_half_scalar(h, f, g, pairCount, gammaOff);
}
#else
__aicore__ inline void multiply_ntts_half_dispatch(LocalTensor<int32_t> &h, LocalTensor<int32_t> &f,
                                                   LocalTensor<int32_t> &g, LocalTensor<int32_t> &basemulWsBase,
                                                   int32_t pairCount, int32_t gammaOff)
{
#if HAT_BASEMUL_VARIANT == 1
    hat_basemul::BasemulWs w;
    hat_basemul::bind_basemul_ws(basemulWsBase, w, static_cast<uint32_t>(pairCount));
    hat_basemul::multiply_ntts_half_deinterleave_vec(h, f, g, w, pairCount, gammaOff);
#elif HAT_BASEMUL_VARIANT == 2
    hat_basemul::BasemulWs w;
    hat_basemul::bind_basemul_ws(basemulWsBase, w, static_cast<uint32_t>(pairCount));
    hat_basemul::multiply_ntts_half_gather_vec(h, f, g, w, pairCount, gammaOff);
#else
    multiply_ntts_half_scalar(h, f, g, pairCount, gammaOff);
#endif
}
#endif

#endif
