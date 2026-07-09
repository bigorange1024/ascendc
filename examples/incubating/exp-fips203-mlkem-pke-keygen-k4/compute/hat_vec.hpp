// @probe exp-fips203-mlkem-pke-keygen-k4
// @file compute/hat_vec.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `hat_vec.hpp` 为该子模块组件。 / Component: hat_vec.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: basic.hpp, hat_gammas.hpp, kernel_operator.h, ntt_vec.hpp, mod_variants.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file hat_vec.hpp
 * @brief 行 18 辅助：Alg.11 系数约化、标量半核 basemul、（已冻结）Gather 向量 basemul 遗留代码。
 *
 * 用途：
 *   - hat_reduce_zq_*：basemul 内 Barrett（与 F203_MOD_VARIANT 无关）；
 *   - multiply_ntts_half_scalar：HAT_ALG11_VEC=0 或回退路径的半 poly basemul；
 *   - coef_pairs_vec / 注释块内 multiply_ntts_half_vec：含 Gather，**已冻结禁止启用**。
 *
 * 调用方：`2s1e_post_ntt_ub.hpp`；生产 basemul 优先 hat_alg11_basemul.hpp（HAT_ALG11_VEC=1）。
 *
 * 不变量：kHatGammas 来自 hat_gammas.hpp；pairCount≤128；交错 f/g 布局与 Alg.11 一致。
 *
 * Golden：hat_inner_product_ref.c；t_hat 整行对拍。
 *
 * CMake：HAT_ALG11_VEC；F203_MOD_VARIANT 经 mod_variants.hpp（本文件 basemul 约化独立）。
 */
#ifndef HAT_VEC_HPP
#define HAT_VEC_HPP

#include "basic.hpp"
#include "hat_gammas.hpp"
#include "kernel_operator.h"
#include "ntt_vec.hpp"
#include "mod_variants.hpp"

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

/** f[halfLen] → a0/a1 各 pairCount 对。** 含 Gather — **已冻结，勿再启用**（见下方注释）。 */
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
 * Alg.11 MultiplyNTTs 半核（向量 Mul/Add，经 coef_pairs_vec/Gather）— **已冻结，禁止再启用**。
 *
 * 项目约束：凡依赖 AscendC::Gather 的路径一律冻结（与平面 mat_c / 无 Gather S3 路线一致）。
 * 历史问题（2026-06）：SIM golden 不一致；PIPE 同步与标量/向量混用。
 *
 * 当前且今后唯一 basemul 交付：multiply_ntts_half_scalar（CPU/SIM 已验收）。
 * 后续加速须在不使用 Gather 前提下进行（如：对已交错 f/g 直接向量 Mul + Barrett reduce）。
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
