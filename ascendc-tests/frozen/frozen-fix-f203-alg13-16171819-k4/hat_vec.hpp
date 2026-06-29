#ifndef HAT_VEC_HPP
#define HAT_VEC_HPP

#include "basic.hpp"
#include "hat_gammas.hpp"
#include "kernel_operator.h"
#include "mod_variants.hpp"
#include "ntt_vec.hpp"

/** Alg.11 basemul 约化：MlkemReduceToZq（与 C ref barrett_red_coeff 一致，不随 F203_MOD_VARIANT 变）。 */
__aicore__ inline int32_t hat_reduce_zq_scalar(int32_t x)
{
    const int32_t q = kHatQ;
    x = x + (q & (x >> 31));
    int32_t t1 = (x * 78) >> 18;
    x = x - t1 * q;
    int32_t t2 = (x * 5039) >> 24;
    x = x - t2 * q;
    x = x - (q & ~((x - q) >> 31));
    return x;
}

__aicore__ inline void hat_reduce_zq_vec(LocalTensor<int32_t> &dst, int32_t count)
{
    for (int32_t i = 0; i < count; ++i) {
        dst.SetValue(i, hat_reduce_zq_scalar(dst.GetValue(i)));
    }
}

/** f[halfLen]（128 系数）→ a0/a1 各 halfLen/2 对 (2i,2i+1)。 */
__aicore__ inline void coef_pairs_vec(LocalTensor<int32_t> &a0, LocalTensor<int32_t> &a1, LocalTensor<int32_t> &f,
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

/** c0/c1[pairCount] → h[halfLen] 交错写回。 */
__aicore__ inline void interleave_pairs_vec(LocalTensor<int32_t> &h, LocalTensor<int32_t> &c0, LocalTensor<int32_t> &c1,
                                            int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        h.SetValue(i * 2, c0.GetValue(i));
        h.SetValue(i * 2 + 1, c1.GetValue(i));
    }
}

/** 标量 BaseCaseMultiply（CPU/SIM/NPU 当前唯一 basemul 路径）。 */
__aicore__ inline void multiply_ntts_half_scalar(LocalTensor<int32_t> &h, LocalTensor<int32_t> &f, LocalTensor<int32_t> &g,
                                                 int32_t pairCount, int32_t gammaOff)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        const int32_t a0 = f.GetValue(i * 2);
        const int32_t a1 = f.GetValue(i * 2 + 1);
        const int32_t b0 = g.GetValue(i * 2);
        const int32_t b1 = g.GetValue(i * 2 + 1);
        const int32_t gval = kHatGammas[gammaOff + i];
        const int32_t a1b1 = hat_reduce_zq_scalar(a1 * b1);
        const int32_t c0 = hat_reduce_zq_scalar(a0 * b0 + a1b1 * gval);
        const int32_t c1 = hat_reduce_zq_scalar(a0 * b1 + a1 * b0);
        h.SetValue(i * 2, c0);
        h.SetValue(i * 2 + 1, c1);
    }
}

/*
 * Alg.11 MultiplyNTTs 半核（向量 Mul/Add + Mlkem Barrett）— **暂未启用**。
 *
 * 问题（2026-06）：SIM 上与 golden 不一致；CPU 可能仍通过（顺序执行掩盖竞态）。
 * 根因：Gather/Mul/Add 走 Vector，hat_reduce_zq_vec、γ 乘、interleave 用 GetValue/SetValue
 * 标量读 UB，PIPE 同步不足时 basemul 输出错，lazy ∑ 随之错（非 final mod 单独问题）。
 *
 * 当前交付：AivHatLine18 仅用 multiply_ntts_half_scalar（CPU/SIM 已验收）。
 * 待修后再取消注释：细粒度 PipeBarrier<PIPE_V>、γ/interleave/reduce 全向量化、缓冲别名梳理。
 *
__aicore__ inline void multiply_ntts_half_vec(LocalTensor<int32_t> &h, LocalTensor<int32_t> &f, LocalTensor<int32_t> &g,
                                              LocalTensor<int32_t> &a0, LocalTensor<int32_t> &a1,
                                              LocalTensor<int32_t> &b0, LocalTensor<int32_t> &b1,
                                              LocalTensor<int32_t> &c0, LocalTensor<int32_t> &c1,
                                              LocalTensor<int32_t> &t1, LocalTensor<int32_t> &t2,
                                              LocalTensor<int32_t> &idx, LocalTensor<int32_t> &idx2,
                                              int32_t pairCount, int32_t gammaOff)
{
    using AscendC::Add;
    using AscendC::Mul;
    coef_pairs_vec(a0, a1, f, idx, idx2, pairCount);
    coef_pairs_vec(b0, b1, g, idx, idx2, pairCount);
    KYBER_PIPE_ALL();

    Mul(t1, a1, b1, pairCount);
    KYBER_PIPE_ALL();
    hat_reduce_zq_vec(t1, pairCount);
    KYBER_PIPE_ALL();
    for (int32_t i = 0; i < pairCount; ++i) {
        const int32_t gval = kHatGammas[gammaOff + i];
        t2.SetValue(i, hat_reduce_zq_scalar(t1.GetValue(i) * gval));
    }
    KYBER_PIPE_ALL();
    Mul(t1, a0, b0, pairCount);
    KYBER_PIPE_ALL();
    Add(c0, t1, t2, pairCount);
    KYBER_PIPE_ALL();
    hat_reduce_zq_vec(c0, pairCount);
    KYBER_PIPE_ALL();
    Mul(t1, a0, b1, pairCount);
    Mul(t2, a1, b0, pairCount);
    KYBER_PIPE_ALL();
    Add(c1, t1, t2, pairCount);
    KYBER_PIPE_ALL();
    hat_reduce_zq_vec(c1, pairCount);
    KYBER_PIPE_ALL();
    interleave_pairs_vec(h, c0, c1, pairCount);
    KYBER_PIPE_ALL();
}
*/

#endif
