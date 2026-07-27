/**
 * @file stage3_mod_variants.hpp
 * @brief Stage3 RouteA Horner 合并后的三种 mod q 实现（与行 18 basemul mod 分离）。
 *
 * 流水线位置：由 ntt_vec.hpp 在 wrap_mod_vec_runtime / combine_limb6_horner_raw_vec 之后 include；
 * 经 combine_limb6_routea_mod_vec 被 AivK8RouteAMod 调用。
 *
 * 作用：提供 Barrett / 标量 int64 / float Div 三种 Stage3.1 取模变体，由 F203_STAGE3_MOD 选择。
 *
 * 不变量：kF203BarrettMu/K；kKyberMergeShift1=6；wrap_mod_vec_runtime 与 Stage3 Barrett 共用。
 * 三段式 NTT 内禁止 Gather——本文件仅做向量算术，不读偶奇交织列。
 *
 * 与 golden 关系：与 gen_data / mlkem_ref.stage31_mod 等价（方案 0）；方案 1 SIM 慢需预算≥20s。
 *
 * CMake：F203_STAGE3_MOD（stage3_config.hpp）。
 *
 * 注意：本头依赖 ntt_vec.hpp 中已定义的 wrap_mod_vec_runtime / combine_limb6_horner_raw_vec，
 * 不可单独 include。
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

/**
 * 单次 Barrett 约化：dst ← dst - q * ((dst * mu) >> k)，再 wrap 到 [0,q)。
 *
 * @param dst   待约化向量（in/out），int32，长度 count
 * @param q     模数，本探针固定 3329
 * @param t1,t2 临时向量，长度 ≥ count
 * @param count 元素个数（通常 halfLen=128）
 * 前置：dst/t1/t2 已在 UB；调用方保证 PIPE 序
 */
__aicore__ inline void barrett_reduce_limb6_vec(LocalTensor<int32_t> &dst, int32_t q, LocalTensor<int32_t> &t1,
                                                LocalTensor<int32_t> &t2, int32_t count)
{
    // t1 = (dst * mu) >> k  → 近似商
    AscendC::Muls(t1, dst, kF203BarrettMu, count);
    AscendC::ShiftRight(t1, t1, kF203BarrettK, count);
    // dst = dst - q * 商
    AscendC::Muls(t2, t1, q, count);
    AscendC::Sub(dst, dst, t2, count);
    // 将可能越界的余数折回 [0,q)
    wrap_mod_vec_runtime(dst, dst, q, t1, t2, count);
}

/**
 * RouteA Horner + 每步 Barrett：acc=hh; acc=acc*64+(hl+lh); acc=acc*64+ll。
 *
 * @param dst          输出半 poly 系数 [halfLen] int32（已 mod q）
 * @param hh,lh,hl,ll  平面四 limb 行，各 [halfLen] int32
 * @param t1,t2        Barrett 临时
 * @param q,count      模数与长度
 * 前置：四 limb 已按 planar 布局拷入 UB；无 Gather
 */
__aicore__ inline void combine_limb6_horner_barrett_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                        LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                        LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                        LocalTensor<int32_t> &t2, int32_t q, int32_t count)
{
    using AscendC::Add;
    using AscendC::DataCopy;
    using AscendC::ShiftLeft;
    // 步 0：从 hh 起步并约化
    DataCopy(dst, hh, static_cast<uint32_t>(count));
    barrett_reduce_limb6_vec(dst, q, t1, t2, count);
    // 步 1：×64 + (hl+lh)
    Add(t1, hl, lh, count);
    ShiftLeft(dst, dst, kKyberMergeShift1, count);
    Add(dst, dst, t1, count);
    barrett_reduce_limb6_vec(dst, q, t1, t2, count);
    // 步 2：×64 + ll
    ShiftLeft(dst, dst, kKyberMergeShift1, count);
    Add(dst, dst, ll, count);
    barrett_reduce_limb6_vec(dst, q, t1, t2, count);
}

// ---------------------------------------------------------------------------
// 方案 1：Horner raw + 标量 int64 floor mod（Stage31ModI64）
// golden：与 mlkem_ref.stage31_mod 一致
// SIM：~16s（标量 GetValue/SetValue 在 PEM 很慢）；需 KERNEL_COMPUTE_BUDGET_SEC≥20
// ---------------------------------------------------------------------------

/**
 * 标量 floor 取模：对每个元素做与 Python stage31_mod 同构的 int64 除法。
 *
 * @param dst   in/out int32 向量
 * @param q     模数
 * @param count 元素个数
 * 前置：dst 已含 Horner raw（可能很大）；仅调试/对照路径使用
 */
__aicore__ inline void stage31_mod_i64_scalar(LocalTensor<int32_t> &dst, int32_t q, int32_t count)
{
    const int64_t q64 = static_cast<int64_t>(q);
    // 逐元素：t = floor(raw/q)（对负 raw 用对称写法），rem = raw - q*t
    for (int32_t i = 0; i < count; i++) {
        const int64_t raw = static_cast<int64_t>(dst.GetValue(i));
        const int64_t t = (raw >= 0) ? (raw / q64) : (-((-raw) / q64));
        dst.SetValue(i, static_cast<int32_t>(raw - q64 * t));
    }
}

/**
 * Horner raw（无中途约化）+ 标量 int64 mod。
 *
 * @param t1  Horner 临时（hl+lh）
 * 其余参数同 combine_limb6_horner_barrett_vec
 */
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

/**
 * float Div 路径取模：dst ← dst - q * trunc(float(dst)/float(q))。
 *
 * @param fRaw,fTmp,fQuot  各 halfLen 的 float 临时（来自 calc_f_ TBuf）
 * @param t1               int32 临时（存 q 副本与商）
 * 前置：F203_STAGE3_MOD==2；AivK8RouteAMod 已分配 calc_f_
 */
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

    // int32 → float 被除数
    Cast(fRaw, dst, AscendC::RoundMode::CAST_NONE, n);
    // KYBER_PIPE_ALL();
    Duplicate(t1, q, count);
    // KYBER_PIPE_ALL();
    Cast(fTmp, t1, AscendC::RoundMode::CAST_NONE, n);
    // KYBER_PIPE_ALL();
    // 商 = raw / q（float），再截断回 int32
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
 * Horner raw + float Div mod（方案 2 入口）。
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
