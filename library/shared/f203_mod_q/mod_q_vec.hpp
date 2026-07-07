#pragma once

/**
 * @file mod_q_vec.hpp
 * @brief FIPS 203 / Kyber q=3329：Z_q 向量 Barrett 约简与 final mod（共享库）。
 *
 * 用途：内积 Σ basemul 后、时域 u+e 等步骤的 mod q；对齐 vec-k4-v2 F203_MOD_VARIANT==1。
 * 调用方：encrypt-compute、halfrows、2s1e_post_ntt_ub（可逐步迁入）。
 *
 * 不变量：μ=314、k=20；wrap_mod_vec_runtime 与 Stage3 Barrett 末步一致。
 * CPU 孪生（ASCENDC_CPU_DEBUG）：mod_q_final_vec 走标量 int64 floor mod。
 */
#include "kernel_operator.h"

namespace f203_mod_q {

constexpr int32_t kKyberQ = 3329;
constexpr int32_t kBarrettMu = 314;
constexpr int32_t kBarrettK = 20;

/** x∈[0,q)：x - (q & ~((x-q)>>31))，与 Alg11 barrett_red 末步一致。 */
__aicore__ inline void wrap_mod_vec_runtime(AscendC::LocalTensor<int32_t> &dst, AscendC::LocalTensor<int32_t> &src,
                                            int32_t q, AscendC::LocalTensor<int32_t> &t1,
                                            AscendC::LocalTensor<int32_t> &t2, int32_t count)
{
    AscendC::Adds(t1, src, -q, count);
    auto &t1_u32 = *reinterpret_cast<AscendC::LocalTensor<uint32_t> *>(&t1);
    auto &t2_u32 = *reinterpret_cast<AscendC::LocalTensor<uint32_t> *>(&t2);
    AscendC::ShiftRight(t2_u32, t1_u32, 31U, count);
    AscendC::Mul(t2, src, t2, count);
    AscendC::Max(dst, t1, t2, count);
}

/** 向量 Barrett：dst ← dst mod q（原地）。 */
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

/** CPU 孪生：逐 lane 标量 int64 mod（对齐 C ref golden）。 */
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

} // namespace f203_mod_q
