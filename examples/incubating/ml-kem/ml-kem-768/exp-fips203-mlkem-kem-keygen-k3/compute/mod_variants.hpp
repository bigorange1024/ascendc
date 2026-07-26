// @probe pass-fix-f203-alg19-kem-keygen-device-k3
// @file compute/mod_variants.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `mod_variants.hpp` 为该子模块组件。 / Component: mod_variants.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_kem.bin (1184B) + dk_kem.bin (2400B)；D13 PKE 中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_kem+dk_kem out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: basic.hpp, kernel_operator.h, kyber_limb6.hpp, mod_config.hpp, stage3_mod_variants.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 模约化变体配置。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-768（k=3）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/mod_variants.hpp
 */
/**
 * @file mod_variants.hpp
 * @brief 行 18 final mod q 的三种设备实现与 MOD_Q_I32 统一入口。
 *
 * 用途：mod_q_scalar_i64_vec / mod_q_barrett_vec / mod_q_cast_div_vec；由 F203_MOD_VARIANT 编译期选择。
 *
 * 调用方：`2s1e_post_ntt_ub.hpp`（内积累加后写 ub 或 GM t_hat）；依赖 stage3_mod_variants 的 wrap_mod_vec_runtime。
 *
 * 不变量：
 *   - Barrett μ=314、k=20；
 *   - C ref golden 固定标量 int64，三种设备变体数学上应与 golden 等价（已 CPU/SIM 验证 Barrett=1）。
 *
 * Golden：golden_t_hat_c.bin / golden_t_hat_dot.bin；verify_result.py。
 *
 * CMake：F203_MOD_VARIANT（mod_config.hpp 默认 1）。
 */
#ifndef F203_MOD_VARIANTS_HPP
#define F203_MOD_VARIANTS_HPP

#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "mod_config.hpp"
#include "stage3_mod_variants.hpp"

// ---------------------------------------------------------------------------
// 行 18 final mod（Σ_j basemul + ê 之后）；由 F203_MOD_VARIANT 选实现，MOD_Q_* 宏统一入口。
// C ref hat_inner_product_add 固定 HAT_MOD_SCALAR_I64，与下列设备变体对拍。
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

/**
 * 本函数为 KeyGen 流水线组件 `mod_q_scalar_i64_vec`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-768（k=3）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void mod_q_scalar_i64_vec(LocalTensor<int32_t> &dst, int32_t q, int32_t count)
{
    for (int32_t i = 0; i < count; ++i) {
        const int64_t raw = static_cast<int64_t>(dst.GetValue(i));
        dst.SetValue(i, mod_q_scalar_i64_one(raw, q));
    }
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

/**
 * 本函数为 KeyGen 流水线组件 `mod_q_cast_div_vec`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-768（k=3）；与 golden 仅 I/O 等价。
 */
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

#if F203_MOD_VARIANT == 0
#define MOD_Q_I32(DST, Q, T1, T2, N) mod_q_scalar_i64_vec((DST), (Q), (N))
#elif F203_MOD_VARIANT == 1
#define MOD_Q_I32(DST, Q, T1, T2, N) mod_q_barrett_vec((DST), (Q), (T1), (T2), (N))
#endif

#if F203_MOD_VARIANT == 2
#define MOD_Q_CAST(DST, Q, T1, FR, FT, FQ, N) mod_q_cast_div_vec((DST), (Q), (T1), (FR), (FT), (FQ), (N))
#endif

#endif
