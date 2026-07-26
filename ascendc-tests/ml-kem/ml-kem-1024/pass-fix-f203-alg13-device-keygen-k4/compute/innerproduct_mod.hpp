// @probe pass-fix-f203-alg13-device-keygen-k4
// @file compute/innerproduct_mod.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `innerproduct_mod.hpp` 为该子模块组件。 / Component: innerproduct_mod.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 内积模约辅助。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/innerproduct_mod.hpp
 */
/**
 * @file innerproduct_mod.hpp
 * @brief 行 18 内积累加后的 final mod q（与 Stage3 mod 解耦）。
 *
 * 用途：Σ_j MultiplyNTTs(Â[p,j], ŝ[j])（+ 可选 ê）之后，将 int32/int64 累加结果约化到 [0,q)。
 *
 * 调用方：`2s1e_post_ntt_ub.hpp`、`hat_dot_halfrows_ub.hpp`；设备侧经 `mod_variants.hpp` 的 MOD_Q_I32 也可路由至此。
 *
 * 不变量：
 *   - Barrett μ=314、k=20（q=3329），与 hat_inner_product_ref.c 中 HAT_MOD_BARRETT 数学等价；
 *   - wrap_mod_vec_runtime 末步与 Alg.11 barrett_red_coeff 一致；
 *   - ASCENDC_CPU_DEBUG 下 mod_q_final_vec 走标量 int64（对齐 golden HAT_MOD_SCALAR_I64）。
 *
 * Golden：gen_data 固定 HAT_GOLDEN_MOD_VARIANT=0（标量 int64）；设备默认 F203_MOD_VARIANT=1（Barrett 向量）。
 *
 * CMake：F203_MOD_VARIANT 见 mod_config.hpp（当前未编入 CMakeLists，改头文件后重编）。
 */
#pragma once

#include "kernel_operator.h"

namespace hat_ip {

constexpr int32_t kBarrettMu = 314;
constexpr int32_t kBarrettK = 20;

/** x ∈ [0,q)：x - (q & ~((x-q)>>31))，与 Alg11 barrett_red 末步一致。 */
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

/** 行 18 final mod：Barrett 向量（对齐 2s1e F203_MOD_VARIANT==1 / MOD_Q_I32）。 */
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
