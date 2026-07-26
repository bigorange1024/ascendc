/**
 * @file innerproduct_mod.hpp
 * @brief 行 18 lazy 累加后的 final mod q（向量 Barrett / CPU 标量）。
 *
 * 设备路径：μ=314、k=20 的 Barrett + wrap，对齐 2s1e F203_MOD_VARIANT==1。
 * CPU 孪生：逐元素 int64 除法取模，对齐 C ref HAT_MOD_SCALAR_I64。
 */
#pragma once

#include "innerproduct_tiling.h"
#include "kernel_operator.h"

namespace hat_ip {

/** Barrett 乘法常数 μ ≈ 2^k / q（k=20, q=3329）。 */
constexpr int32_t kBarrettMu = 314;
constexpr int32_t kBarrettK = 20;

/**
 * 向量 wrap：把可能 ≥q 的值压回 [0,q)。
 * 实现：t1=src-q；符号掩码选 max(src-q, src&mask)，与 Alg11 barrett_red 末步一致。
 * @param dst   输出
 * @param src   输入（可与 dst 同缓冲）
 * @param q     模数 3329
 * @param t1,t2 临时 LocalTensor，长度 ≥ count
 * @param count 元素个数
 */
__aicore__ inline void wrap_mod_vec_runtime(AscendC::LocalTensor<int32_t> &dst, AscendC::LocalTensor<int32_t> &src,
                                            int32_t q, AscendC::LocalTensor<int32_t> &t1,
                                            AscendC::LocalTensor<int32_t> &t2, int32_t count)
{
    using AscendC::Adds;
    using AscendC::Max;
    using AscendC::Mul;
    using AscendC::ShiftRight;

    // t1 = src - q；算术右移 31 得符号掩码（≥q 时为 0，否则为全 1）
    Adds(t1, src, -q, count);
    auto &t1_u32 = *reinterpret_cast<AscendC::LocalTensor<uint32_t> *>(&t1);
    auto &t2_u32 = *reinterpret_cast<AscendC::LocalTensor<uint32_t> *>(&t2);
    ShiftRight(t2_u32, t1_u32, 31U, count);
    Mul(t2, src, t2, count);
    Max(dst, t1, t2, count);
}

/**
 * 行 18 final mod：向量 Barrett（对齐 2s1e F203_MOD_VARIANT==1 / MOD_Q_I32）。
 * @param dst   就地约减的累加行
 * @param q     模数
 * @param t1,t2 临时缓冲
 * @param count 系数个数（通常 N=256）
 */
__aicore__ inline void mod_q_barrett_vec(AscendC::LocalTensor<int32_t> &dst, int32_t q,
                                         AscendC::LocalTensor<int32_t> &t1, AscendC::LocalTensor<int32_t> &t2,
                                         int32_t count)
{
    // q_hat = (dst * μ) >> k；dst -= q_hat * q；再 wrap
    AscendC::Muls(t1, dst, kBarrettMu, count);
    AscendC::ShiftRight(t1, t1, kBarrettK, count);
    AscendC::Muls(t2, t1, q, count);
    AscendC::Sub(dst, dst, t2, count);
    wrap_mod_vec_runtime(dst, dst, q, t1, t2, count);
}

#if defined(ASCENDC_CPU_DEBUG)
/**
 * 单元素 int64 向零除法取模，结果规范到 [0,q)。
 */
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

/**
 * CPU 孪生调试：标量 int64 mod（与 C ref HAT_MOD_SCALAR_I64 一致）。
 * @param dst   就地改写的 LocalTensor
 * @param q     模数
 * @param count 元素个数
 */
__aicore__ inline void mod_q_final_vec(AscendC::LocalTensor<int32_t> &dst, int32_t q, int32_t count)
{
    for (int32_t i = 0; i < count; ++i) {
        const int64_t raw = static_cast<int64_t>(dst.GetValue(static_cast<uint32_t>(i)));
        dst.SetValue(static_cast<uint32_t>(i), mod_q_scalar_i64_one(raw, q));
    }
}
#else
/**
 * 设备 / SIM：向量 Barrett final mod（需额外 t1/t2 临时）。
 */
__aicore__ inline void mod_q_final_vec(AscendC::LocalTensor<int32_t> &dst, int32_t q,
                                       AscendC::LocalTensor<int32_t> &t1, AscendC::LocalTensor<int32_t> &t2,
                                       int32_t count)
{
    mod_q_barrett_vec(dst, q, t1, t2, count);
}
#endif

} // namespace hat_ip
