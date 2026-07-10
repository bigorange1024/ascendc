/**
 * @file innerproduct_mod.hpp
 * @brief 内积 / Alg.11 累加后的向量 mod q（Barrett）与 CPU 孪生标量对照。
 *
 * 流水线位置：su_dot 写出 ŵ 前的 final mod；与 KeyGen 行 18 同族。
 * 设备/SIM：mod_q_barrett_vec；CPU debug：int64 对称除法（对齐 C ref）。
 */
#pragma once

#include "innerproduct_tiling.h"
#include "kernel_operator.h"

namespace hat_ip {

constexpr int32_t kBarrettMu = 314;
constexpr int32_t kBarrettK = 20;

/**
 * 把已约减到附近的 x 折回 [0,q)：x - (q & ~((x-q)>>31))。
 * 与 Alg11 barrett_red 末步一致。
 */
__aicore__ inline void wrap_mod_vec_runtime(AscendC::LocalTensor<int32_t> &dst, AscendC::LocalTensor<int32_t> &src,
                                            int32_t q, AscendC::LocalTensor<int32_t> &t1,
                                            AscendC::LocalTensor<int32_t> &t2, int32_t count)
{
    using AscendC::Adds;
    using AscendC::Max;
    using AscendC::Mul;
    using AscendC::ShiftRight;

    Adds(t1, src, -q, count);
    auto &t1_u32 = *reinterpret_cast<AscendC::LocalTensor<uint32_t> *>(&t1);
    auto &t2_u32 = *reinterpret_cast<AscendC::LocalTensor<uint32_t> *>(&t2);
    ShiftRight(t2_u32, t1_u32, 31U, count);
    Mul(t2, src, t2, count);
    Max(dst, t1, t2, count);
}

/**
 * 行 18 / su_dot final mod：向量 Barrett（mu=314,k=20）。
 * @param dst 就地约减；t1/t2 临时 UB
 */
__aicore__ inline void mod_q_barrett_vec(AscendC::LocalTensor<int32_t> &dst, int32_t q,
                                         AscendC::LocalTensor<int32_t> &t1, AscendC::LocalTensor<int32_t> &t2,
                                         int32_t count)
{
    AscendC::Muls(t1, dst, kBarrettMu, count);
    AscendC::ShiftRight(t1, t1, kBarrettK, count);
    AscendC::Muls(t2, t1, q, count);
    AscendC::Sub(dst, dst, t2, count);
    wrap_mod_vec_runtime(dst, dst, q, t1, t2, count);
}

#if defined(ASCENDC_CPU_DEBUG)
/** 单元素 int64 对称 mod（CPU 孪生）。 */
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

/** CPU 孪生调试：标量 int64 mod（与 C ref HAT_MOD_SCALAR_I64 一致）。 */
__aicore__ inline void mod_q_final_vec(AscendC::LocalTensor<int32_t> &dst, int32_t q, int32_t count)
{
    for (int32_t i = 0; i < count; ++i) {
        const int64_t raw = static_cast<int64_t>(dst.GetValue(static_cast<uint32_t>(i)));
        dst.SetValue(static_cast<uint32_t>(i), mod_q_scalar_i64_one(raw, q));
    }
}
#else
/** 设备 / SIM：向量 Barrett final mod。 */
__aicore__ inline void mod_q_final_vec(AscendC::LocalTensor<int32_t> &dst, int32_t q,
                                       AscendC::LocalTensor<int32_t> &t1, AscendC::LocalTensor<int32_t> &t2,
                                       int32_t count)
{
    mod_q_barrett_vec(dst, q, t1, t2, count);
}
#endif

} // namespace hat_ip
