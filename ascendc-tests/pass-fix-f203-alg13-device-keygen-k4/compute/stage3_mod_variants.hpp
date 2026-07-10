// @probe pass-fix-f203-alg13-device-keygen-k4
// @file compute/stage3_mod_variants.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `stage3_mod_variants.hpp` 为该子模块组件。 / Component: stage3_mod_variants.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends compute 树内互引；host 经 main/mmad_custom 与 tiling.h 链接。
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 Stage1/3 编译期配置开关。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/stage3_mod_variants.hpp
 */
/**
 * @file stage3_mod_variants.hpp
 * @brief Stage3 RouteA Horner 合并后的三种 mod q 实现（与行 18 mod 分离）。
 *
 * 用途：由 ntt_vec.hpp 在 combine_limb6_horner_raw_vec 之后 include；提供 barrett / scalar_i64 / cast_div 变体。
 *
 * 调用方：ntt_vec.hpp::combine_limb6_routea_mod_vec（F203_STAGE3_MOD 选择）。
 *
 * 不变量：kF203BarrettMu/K；kKyberMergeShift1=6；wrap_mod_vec_runtime 与 Stage3 Barrett 共用。
 *
 * Golden：与 gen_data stage31_mod 等价（方案 0）；方案 1 SIM 慢需 KERNEL_COMPUTE_BUDGET_SEC≥20。
 *
 * CMake：F203_STAGE3_MOD（stage3_config.hpp）。
 */
#ifndef F203_STAGE3_MOD_VARIANTS_HPP
#define F203_STAGE3_MOD_VARIANTS_HPP

/* 由 ntt_vec.hpp 在 wrap_mod_vec_runtime / combine_limb6_horner_raw_vec 之后 include */

/** Barrett 参数：q=3329，见 ntt_study mlkem_ntt_tables.h */
constexpr int32_t kF203BarrettMu = 314;
constexpr int32_t kF203BarrettK = 20;

// ---------------------------------------------------------------------------
// 方案 0：Barrett 三步 Horner（合并+约化融合，全程 int32 向量）
// golden：与本 testcase 的 stage31_mod 数学等价（已 CPU/SIM 验证过）
// SIM：~10.5s 全链路
// ---------------------------------------------------------------------------

__aicore__ inline void barrett_reduce_limb6_vec(LocalTensor<int32_t> &dst, int32_t q, LocalTensor<int32_t> &t1,
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
    barrett_reduce_limb6_vec(dst, q, t1, t2, count);
    Add(t1, hl, lh, count);
    ShiftLeft(dst, dst, kKyberMergeShift1, count);
    Add(dst, dst, t1, count);
    barrett_reduce_limb6_vec(dst, q, t1, t2, count);
    ShiftLeft(dst, dst, kKyberMergeShift1, count);
    Add(dst, dst, ll, count);
    barrett_reduce_limb6_vec(dst, q, t1, t2, count);
}

// ---------------------------------------------------------------------------
// 方案 1：Horner raw + 标量 int64 floor mod（Stage31ModI64）
// golden：与 mlkem_ref.stage31_mod 一致
// SIM：~16s（标量 GetValue/SetValue 在 PEM 很慢）；需 KERNEL_COMPUTE_BUDGET_SEC≥20
// ---------------------------------------------------------------------------

__aicore__ inline void stage31_mod_i64_scalar(LocalTensor<int32_t> &dst, int32_t q, int32_t count)
{
    const int64_t q64 = static_cast<int64_t>(q);
    for (int32_t i = 0; i < count; i++) {
        const int64_t raw = static_cast<int64_t>(dst.GetValue(i));
        const int64_t t = (raw >= 0) ? (raw / q64) : (-((-raw) / q64));
        dst.SetValue(i, static_cast<int32_t>(raw - q64 * t));
    }
}

__aicore__ inline void combine_limb6_routea_mod_scalar_i64(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                           LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                           LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                           int32_t q, int32_t count)
{
    combine_limb6_horner_raw_vec(dst, hh, lh, hl, ll, t1, count);
    stage31_mod_i64_scalar(dst, q, count);
}

// ---------------------------------------------------------------------------
// 方案 2：Horner raw + Cast→float Div→Cast 商→int32 Muls/Sub（ONNX Stage3.1）
// golden：与本 testcase 一致（本数据上 float Div 与 int floor 商一致）
// SIM：~10s；UB：scratch_t1 + calc_f(3×halfLen float)，勿再堆 VECIN TQue（上限 8）
// ---------------------------------------------------------------------------

__aicore__ inline void stage31_div_mod_vec(LocalTensor<int32_t> &dst, int32_t q, LocalTensor<int32_t> &t1,
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
    // KYBER_PIPE_ALL();
    Duplicate(t1, q, count);
    // KYBER_PIPE_ALL();
    Cast(fTmp, t1, AscendC::RoundMode::CAST_NONE, n);
    // KYBER_PIPE_ALL();
    Div(fQuot, fRaw, fTmp, count);
    // KYBER_PIPE_ALL();
    Cast(t1, fQuot, AscendC::RoundMode::CAST_TRUNC, n);
    // KYBER_PIPE_ALL();
    Muls(t1, t1, q, count);
    // KYBER_PIPE_ALL();
    Sub(dst, dst, t1, count);
    // KYBER_PIPE_ALL();
}

/**
 * 本函数为 KeyGen 流水线组件 `combine_limb6_routea_mod_cast_div`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void combine_limb6_routea_mod_cast_div(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                         LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                         LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                         LocalTensor<float> &fRaw, LocalTensor<float> &fTmp,
                                                         LocalTensor<float> &fQuot, int32_t q, int32_t count)
{
    combine_limb6_horner_raw_vec(dst, hh, lh, hl, ll, t1, count);
    stage31_div_mod_vec(dst, q, t1, fRaw, fTmp, fQuot, count);
}

#endif
