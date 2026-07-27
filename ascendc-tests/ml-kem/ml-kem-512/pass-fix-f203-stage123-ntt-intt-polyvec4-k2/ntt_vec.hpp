/**
 * @file ntt_vec.hpp
 * @brief F203 Tag5T Stage1 limb6 分裂与 Stage3 RouteA 平面 merge/mod（无 Gather）。
 *
 * 流水线位置：
 *   - Stage1：AivK4Split.encodeBank → split_vec → S0 [8,256] int8；
 *   - Stage3：AivK4RouteAMod → combine_limb6_routea_mod_vec → dst [4,256] int32。
 *
 * 作用：
 *   - Stage1：int32→(lo,hi) 各 6-bit limb（F203_STAGE1_SPLIT 0/1/2）；
 *   - Stage3：平面读 mat_c 四行 Horner 合并 + mod q（F203_STAGE3_MOD 选变体）。
 *
 * 不变量：
 *   - kKyberLimbBits=6；lo = v - hi*64（向量路径 Sub，非 And）；
 *   - RouteA 合并 shift=6；Stage3 内禁止 Gather（项目 ML-KEM Tag5T 约束）；
 *   - poly-batch：调用方保证每 AIV 握有完整 poly 的 hi+lo。
 *
 * 与 golden 关系：gen_data 平面 mat_c + stage31_mod 语义；dst.bin [4,256] NTT/INTT 后系数。
 *
 * CMake：F203_STAGE1_SPLIT、F203_STAGE3_MOD（stage1/3_config.hpp）。
 */
#ifndef __NTT_VEC_HPP__
#define __NTT_VEC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "stage1_config.hpp"
#include "stage3_config.hpp"

/** Stage1 输出对：x0=lo limbs，x1=hi limbs（各与 src 同长度的 int8） */
struct Tensor_int8x4 {
    LocalTensor<int8_t> &x0;
    LocalTensor<int8_t> &x1;
};

/**
 * LocalTensor 类型重解释（不改底层存储，仅改视图类型）。
 * @param x  源 LocalTensor
 * @return   同缓冲的 U 类型视图
 */
template <typename U, typename T>
__aicore__ static inline auto tr(LocalTensor<T> x)
{
    return x.template ReinterpretCast<U>();
}

/**
 * 标量：将一个 int32 系数拆成 lo/hi 两个 6-bit int8。
 * @param d0  lo = a & 0x3f
 * @param d1  hi = (a >> 6) & 0x3f
 * @param a   输入系数（调用方保证已在 Z_q 非负时与 golden 一致）
 */
__aicore__ static inline void split_2xint6(int8_t &d0, int8_t &d1, int32_t a)
{
    d0 = static_cast<int8_t>(a & kKyberLimbMask);
    d1 = static_cast<int8_t>((a >> kKyberLimbBits) & kKyberLimbMask);
}

/**
 * Stage1 标量路径：按 4 个 int32 一组拆 limb，打包写入 int8 缓冲。
 *
 * @param dst   x0←lo、x1←hi
 * @param src   [count] int32 系数
 * @param count 元素个数，须为 4 的倍数（本探针单 AIV bank=2*256）
 * 前置：F203_STAGE1_SPLIT==0 或作为 fallback
 */
__aicore__ inline void split_vec_scalar(Tensor_int8x4 &dst, LocalTensor<int32_t> &src, const int32_t count)
{
    // 将 int8 输出缓冲按 int32 视图写入，一次落 4 个 limb
    auto x0 = tr<int32_t>(dst.x0);
    auto x1 = tr<int32_t>(dst.x1);

    for (int32_t i = 0; i < count / 4; i++) {
        // 取 4 个连续系数
        int32_t a[4] = {src.GetValue(i * 4 + 0), src.GetValue(i * 4 + 1), src.GetValue(i * 4 + 2),
                        src.GetValue(i * 4 + 3)};

        int8_t d0[4], d1[4];
        for (int j = 0; j < 4; j++) {
            split_2xint6(d0[j], d1[j], a[j]);
        }
        // 4×int8 打包成一个 int32 写入
        x0.SetValue(i, *(int32_t *)d0);
        x1.SetValue(i, *(int32_t *)d1);
    }
}

#if F203_STAGE1_SPLIT >= 1

namespace split_vec_detail {

/** 64 = 2^6，用于 lo = v - hi*64 */
constexpr int32_t kLimbScale = static_cast<int32_t>(1) << kKyberLimbBits;

/**
 * int32 → int8 三级 Cast（经 int16、half），满足 AscendC 类型转换约束。
 * @param dst,src,tmpI16,tmpHalf  长度均为 count
 */
__aicore__ inline void cast_i32_to_i8(LocalTensor<int8_t> &dst, LocalTensor<int32_t> &src,
                                      LocalTensor<int16_t> &tmpI16, LocalTensor<half> &tmpHalf, int32_t count)
{
    AscendC::Cast(tmpI16, src, AscendC::RoundMode::CAST_NONE, static_cast<uint32_t>(count));
    KYBER_PIPE_ALL();
    AscendC::Cast(tmpHalf, tmpI16, AscendC::RoundMode::CAST_NONE, static_cast<uint32_t>(count));
    KYBER_PIPE_ALL();
    AscendC::Cast(dst, tmpHalf, AscendC::RoundMode::CAST_NONE, static_cast<uint32_t>(count));
    KYBER_PIPE_ALL();
}

/**
 * 向量 limb6：hi = v>>6，lo = v - hi*64（Sub，禁止用 And 替代）。
 *
 * @param hi,lo  输出
 * @param v      输入系数
 * @param tmp    存 hi*64
 * @param count  长度
 */
__aicore__ inline void limb6_hi_lo_i32(LocalTensor<int32_t> &hi, LocalTensor<int32_t> &lo, LocalTensor<int32_t> &v,
                                       LocalTensor<int32_t> &tmp, int32_t count)
{
    AscendC::ShiftRight(hi, v, kKyberLimbBits, count);
    KYBER_PIPE_ALL();
    AscendC::Muls(tmp, hi, kLimbScale, count);
    KYBER_PIPE_ALL();
    // 关键：lo 必须用减法，与 golden「v - hi*64」一致（非 And）
    AscendC::Sub(lo, v, tmp, count);
    KYBER_PIPE_ALL();
}

/**
 * 对 src[off .. off+tileLen) 做一次 limb6 分裂并 Cast 到 int8。
 *
 * @param off,tileLen  本 tile 在 bank 内的偏移与长度
 * @param scratchI32   至少 3×tileLen int32（hi/lo/tmp）
 */
__aicore__ inline void limb6_tile(Tensor_int8x4 &dst, LocalTensor<int32_t> &src, int32_t off, int32_t tileLen,
                                  LocalTensor<int32_t> &scratchI32, LocalTensor<int16_t> &scratchI16,
                                  LocalTensor<half> &scratchHalf)
{
    LocalTensor<int32_t> hi = scratchI32;
    LocalTensor<int32_t> lo = scratchI32[tileLen];
    LocalTensor<int32_t> tmp = scratchI32[tileLen * 2];

    LocalTensor<int32_t> vTile = src[off];
    LocalTensor<int8_t> loOut = dst.x0[off];
    LocalTensor<int8_t> hiOut = dst.x1[off];

    limb6_hi_lo_i32(hi, lo, vTile, tmp, tileLen);
    cast_i32_to_i8(hiOut, hi, scratchI16, scratchHalf, tileLen);
    cast_i32_to_i8(loOut, lo, scratchI16, scratchHalf, tileLen);
}

} // namespace split_vec_detail

/**
 * 方案 1：整块向量 split（一次 count 个系数）。
 * scratch TBuf 至少 3×count int32 + count int16 + count half。
 *
 * @param dst,src,count,scratch  见上
 */
__aicore__ inline void split_vec_bulk(Tensor_int8x4 &dst, LocalTensor<int32_t> &src, const int32_t count,
                                      AscendC::TBuf<AscendC::TPosition::VECCALC> &scratch)
{
    using split_vec_detail::cast_i32_to_i8;
    using split_vec_detail::limb6_hi_lo_i32;

    const uint32_t n = static_cast<uint32_t>(count);
    const uint32_t i32Bytes = n * static_cast<uint32_t>(sizeof(int32_t));
    const uint32_t i16Bytes = n * static_cast<uint32_t>(sizeof(int16_t));

    // 在同一 TBuf 上切出 hi/lo/tmp/i16/half 视图
    LocalTensor<int32_t> hi = scratch.Get<int32_t>(n);
    LocalTensor<int32_t> lo = scratch.GetWithOffset<int32_t>(n, i32Bytes);
    LocalTensor<int32_t> tmp = scratch.GetWithOffset<int32_t>(n, 2U * i32Bytes);
    LocalTensor<int16_t> tI16 = scratch.GetWithOffset<int16_t>(n, 3U * i32Bytes);
    LocalTensor<half> tHalf = scratch.GetWithOffset<half>(n, 3U * i32Bytes + i16Bytes);

    limb6_hi_lo_i32(hi, lo, src, tmp, count);
    cast_i32_to_i8(dst.x1, hi, tI16, tHalf, count);
    cast_i32_to_i8(dst.x0, lo, tI16, tHalf, count);
}

/**
 * 方案 2：按 tileLen（通常 32）分块向量 split。scratch 按单 tile 尺寸分配。
 */
__aicore__ inline void split_vec_tile(Tensor_int8x4 &dst, LocalTensor<int32_t> &src, const int32_t count,
                                      AscendC::TBuf<AscendC::TPosition::VECCALC> &scratch, const int32_t tileLen)
{
    using split_vec_detail::limb6_tile;

    const uint32_t n = static_cast<uint32_t>(tileLen);
    const uint32_t i32Bytes = n * static_cast<uint32_t>(sizeof(int32_t));
    const uint32_t i16Bytes = n * static_cast<uint32_t>(sizeof(int16_t));

    LocalTensor<int32_t> scratchI32 = scratch.Get<int32_t>(n * 3U);
    LocalTensor<int16_t> scratchI16 = scratch.GetWithOffset<int16_t>(n, 3U * i32Bytes);
    LocalTensor<half> scratchHalf = scratch.GetWithOffset<half>(n, 3U * i32Bytes + i16Bytes);

    // 按 tile 推进；末块可能短于 tileLen
    for (int32_t off = 0; off < count; off += tileLen) {
        const int32_t chunk = (off + tileLen <= count) ? tileLen : (count - off);
        limb6_tile(dst, src, off, chunk, scratchI32, scratchI16, scratchHalf);
    }
}

#endif // F203_STAGE1_SPLIT >= 1

/**
 * Stage1 入口（无 scratch）：仅当 F203_STAGE1_SPLIT==0 时真正执行标量 split。
 * 向量档位须走带 scratch 的重载。
 */
__aicore__ inline void split_vec(Tensor_int8x4 &dst, LocalTensor<int32_t> &src, const int32_t count)
{
#if F203_STAGE1_SPLIT == 0
    split_vec_scalar(dst, src, count);
#else
    (void)dst;
    (void)src;
    (void)count;
#endif
}

/**
 * Stage1 入口（带 scratch）：按 F203_STAGE1_SPLIT 分发 bulk / tile / scalar。
 *
 * @param tileLen  方案 2 的 tile；方案 1 忽略
 */
__aicore__ inline void split_vec(Tensor_int8x4 &dst, LocalTensor<int32_t> &src, const int32_t count,
                                 AscendC::TBuf<AscendC::TPosition::VECCALC> &scratch, const int32_t tileLen)
{
#if F203_STAGE1_SPLIT == 1
    (void)tileLen;
    split_vec_bulk(dst, src, count, scratch);
#elif F203_STAGE1_SPLIT == 2
    split_vec_tile(dst, src, count, scratch, tileLen);
#else
    (void)scratch;
    (void)tileLen;
    split_vec_scalar(dst, src, count);
#endif
}

/**
 * 将可能 ≥q 或 <0 的余数折回 [0,q)：用符号位选择 src 或 src-q。
 *
 * @param dst  输出
 * @param src  输入（可与 dst 同缓冲）
 * @param q    模数
 * @param t1,t2 临时
 * @param count 长度
 */
__aicore__ inline void wrap_mod_vec_runtime(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &src, int32_t q,
                                            LocalTensor<int32_t> &t1, LocalTensor<int32_t> &t2, int32_t count)
{
    AscendC::Adds(t1, src, -q, count);
    auto &t1_u32 = *reinterpret_cast<LocalTensor<uint32_t> *>(&t1);
    auto &t2_u32 = *reinterpret_cast<LocalTensor<uint32_t> *>(&t2);
    // 算术右移取符号掩码，再与 src 相乘得到「若负则保留 src」分支
    AscendC::ShiftRight(t2_u32, t1_u32, 31U, count);
    AscendC::Mul(t2, src, t2, count);
    AscendC::Max(dst, t1, t2, count);
}

/**
 * Barrett 乘法约化变体（本探针 Stage3 主路径用 barrett_reduce_limb6_vec；本函数保留供对照）。
 */
__aicore__ inline void barrett_mul_vec_runtime(LocalTensor<int32_t> &dst, int32_t q, int32_t k, int32_t mu,
                                               LocalTensor<int32_t> &t1, LocalTensor<int32_t> &t2, int32_t count)
{
    AscendC::ShiftRight(t1, dst, static_cast<int32_t>(k - 1), count);
    AscendC::Muls(t1, t1, mu, count);
    AscendC::ShiftRight(t1, t1, static_cast<int32_t>(k + 1), count);
    AscendC::Muls(t1, t1, q, count);
    AscendC::Sub(dst, dst, t1, count);
    wrap_mod_vec_runtime(dst, dst, q, t1, t2, count);
}

/**
 * Stage3 RouteA Horner raw（无中途 mod）：dst = ((hh<<6) + (hl+lh))<<6 + ll。
 * 平面读数，无 Gather。
 *
 * @param hh,lh,hl,ll  四 limb，各 count 个 int32
 * @param t1           存 hl+lh
 * @param count        通常 halfLen=128
 */
__aicore__ inline void combine_limb6_horner_raw_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                    LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                    LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1, int32_t count)
{
    using AscendC::Add;
    using AscendC::ShiftLeft;
    ShiftLeft(dst, hh, kKyberMergeShift1, count);
    Add(t1, hl, lh, count);
    Add(dst, dst, t1, count);
    ShiftLeft(dst, dst, kKyberMergeShift1, count);
    Add(dst, dst, ll, count);
}

#include "stage3_mod_variants.hpp"

#if F203_STAGE3_MOD == 0
/**
 * Stage3 统一入口（方案 0）：Horner+Barrett。
 * @param q  本探针固定传 3329
 */
__aicore__ inline void combine_limb6_routea_mod_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                    LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                    LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                    LocalTensor<int32_t> &t2, int32_t q, int32_t count)
{
    combine_limb6_horner_barrett_vec(dst, hh, lh, hl, ll, t1, t2, q, count);
}
#elif F203_STAGE3_MOD == 1
/** Stage3 统一入口（方案 1）：Horner raw + 标量 int64 mod；t2 未用。 */
__aicore__ inline void combine_limb6_routea_mod_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                    LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                    LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                    LocalTensor<int32_t> &t2, int32_t q, int32_t count)
{
    (void)t2;
    combine_limb6_routea_mod_scalar_i64(dst, hh, lh, hl, ll, t1, q, count);
}
#else
/** Stage3 统一入口（方案 2）：Horner raw + float Div；签名含 float 临时。 */
__aicore__ inline void combine_limb6_routea_mod_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                    LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                    LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                    LocalTensor<float> &fRaw, LocalTensor<float> &fTmp,
                                                    LocalTensor<float> &fQuot, int32_t q, int32_t count)
{
    combine_limb6_routea_mod_cast_div(dst, hh, lh, hl, ll, t1, fRaw, fTmp, fQuot, q, count);
}
#endif

#endif
