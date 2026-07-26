/**
 * @file ntt_vec.hpp
 * @brief Stage1 `split_vec` / Stage3 `combine_limb6_routea_mod_vec` 向量原语。
 *
 * ## 流水线位置
 * FIPS 203 Alg.14 / ML-KEM-1024 Encrypt NTT/INTT S1–S3（AIV 侧）。
 *
 * ## 与 golden
 * 与 host `stage123_transform` I/O 等价；**S1–S3 内禁止 Gather**。
 *
 * ## 职责
 * - Stage1：int32 系数 → 两路 int8 limb（LO/HI，6-bit mask）
 * - Stage3：四路 limb Horner 合并 + mod 3329（`F203_STAGE3_MOD` 选变体）
 * - 辅助：`wrap_mod_vec_runtime` / `barrett_mul_vec_runtime`
 */
#ifndef __NTT_VEC_HPP__
#define __NTT_VEC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "stage1_config.hpp"
#include "stage3_config.hpp"

/** Stage1 输出：x0=LO limb 行，x1=HI limb 行。 */
struct Tensor_int8x4 {
    LocalTensor<int8_t> &x0;
    LocalTensor<int8_t> &x1;
};

/** LocalTensor 类型重解释（不改数据，仅改视图）。 */
template <typename U, typename T>
__aicore__ static inline auto tr(LocalTensor<T> x)
{
    return x.template ReinterpretCast<U>();
}

/**
 * 单系数拆成 LO/HI 两个 6-bit limb（存 int8）。
 * @param d0 LO = a & 0x3f；@param d1 HI = (a>>6) & 0x3f
 */
__aicore__ static inline void split_2xint6(int8_t &d0, int8_t &d1, int32_t a)
{
    d0 = static_cast<int8_t>(a & kKyberLimbMask);
    d1 = static_cast<int8_t>((a >> kKyberLimbBits) & kKyberLimbMask);
}

/**
 * Stage1 标量路径：每 4 个 int32 打包写回 LO/HI 行（F203_STAGE1_SPLIT==0）。
 * @param dst.x0 LO；@param dst.x1 HI；@param count 系数个数（须为 4 的倍数）
 */
__aicore__ inline void split_vec_scalar(Tensor_int8x4 &dst, LocalTensor<int32_t> &src, const int32_t count)
{
    auto x0 = tr<int32_t>(dst.x0);
    auto x1 = tr<int32_t>(dst.x1);

    // 按 4 系数一组：拆 limb 后以 int32 视图写回（4×int8）
    for (int32_t i = 0; i < count / 4; i++) {
        int32_t a[4] = {src.GetValue(i * 4 + 0), src.GetValue(i * 4 + 1), src.GetValue(i * 4 + 2),
                        src.GetValue(i * 4 + 3)};

        int8_t d0[4], d1[4];
        for (int j = 0; j < 4; j++) {
            split_2xint6(d0[j], d1[j], a[j]);
        }
        x0.SetValue(i, *(int32_t *)d0);
        x1.SetValue(i, *(int32_t *)d1);
    }
}

#if F203_STAGE1_SPLIT >= 1

namespace split_vec_detail {

constexpr int32_t kLimbScale = static_cast<int32_t>(1) << kKyberLimbBits;

/**
 * int32 → int8 的 Cast 链（经 int16、half）；A2 上无直接 i32→i8 时的标准路径。
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
 * 向量拆 limb：hi = v>>6；lo = v - hi*64（非 And(v,63)，与 poly-batch 契约一致）。
 */
__aicore__ inline void limb6_hi_lo_i32(LocalTensor<int32_t> &hi, LocalTensor<int32_t> &lo, LocalTensor<int32_t> &v,
                                       LocalTensor<int32_t> &tmp, int32_t count)
{
    AscendC::ShiftRight(hi, v, kKyberLimbBits, count);
    KYBER_PIPE_ALL();
    AscendC::Muls(tmp, hi, kLimbScale, count);
    KYBER_PIPE_ALL();
    AscendC::Sub(lo, v, tmp, count);
    KYBER_PIPE_ALL();
}

/**
 * 对 [off, off+tileLen) 一段系数做 limb 拆分并 Cast 到 dst LO/HI。
 * scratchI32 布局：hi | lo | tmp，各 tileLen。
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

/** 方案 1：整块向量 split（一次 count 个系数）。scratch TBuf 至少 3×count int32 + count int16 + count half。 */
__aicore__ inline void split_vec_bulk(Tensor_int8x4 &dst, LocalTensor<int32_t> &src, const int32_t count,
                                      AscendC::TBuf<AscendC::TPosition::VECCALC> &scratch)
{
    using split_vec_detail::cast_i32_to_i8;
    using split_vec_detail::limb6_hi_lo_i32;

    const uint32_t n = static_cast<uint32_t>(count);
    const uint32_t i32Bytes = n * static_cast<uint32_t>(sizeof(int32_t));
    const uint32_t i16Bytes = n * static_cast<uint32_t>(sizeof(int16_t));

    LocalTensor<int32_t> hi = scratch.Get<int32_t>(n);
    LocalTensor<int32_t> lo = scratch.GetWithOffset<int32_t>(n, i32Bytes);
    LocalTensor<int32_t> tmp = scratch.GetWithOffset<int32_t>(n, 2U * i32Bytes);
    LocalTensor<int16_t> tI16 = scratch.GetWithOffset<int16_t>(n, 3U * i32Bytes);
    LocalTensor<half> tHalf = scratch.GetWithOffset<half>(n, 3U * i32Bytes + i16Bytes);

    limb6_hi_lo_i32(hi, lo, src, tmp, count);
    cast_i32_to_i8(dst.x1, hi, tI16, tHalf, count);
    cast_i32_to_i8(dst.x0, lo, tI16, tHalf, count);
}

/** 方案 2：按 tileLen（通常 32）分块向量 split。scratch TBuf 按单 tile 尺寸分配。 */
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

    for (int32_t off = 0; off < count; off += tileLen) {
        const int32_t chunk = (off + tileLen <= count) ? tileLen : (count - off);
        limb6_tile(dst, src, off, chunk, scratchI32, scratchI16, scratchHalf);
    }
}

#endif // F203_STAGE1_SPLIT >= 1

/**
 * Stage1 入口（无 scratch）：仅 SPLIT==0 走标量；向量模式须用带 scratch 重载。
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
 * Stage1 入口（带 scratch）：按 F203_STAGE1_SPLIT 分派 bulk / tile / scalar。
 * @param tileLen 分块长度（方案 2，通常 32）
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
 * 无分支 wrap 到 [0,q)：若 x>=q 则 x-q，否则保留 x（用符号位掩码实现）。
 */
__aicore__ inline void wrap_mod_vec_runtime(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &src, int32_t q,
                                            LocalTensor<int32_t> &t1, LocalTensor<int32_t> &t2, int32_t count)
{
    AscendC::Adds(t1, src, -q, count);
    auto &t1_u32 = *reinterpret_cast<LocalTensor<uint32_t> *>(&t1);
    auto &t2_u32 = *reinterpret_cast<LocalTensor<uint32_t> *>(&t2);
    AscendC::ShiftRight(t2_u32, t1_u32, 31U, count);
    AscendC::Mul(t2, src, t2, count);
    AscendC::Max(dst, t1, t2, count);
}

/**
 * Barrett 约化一步：dst -= ((dst>>(k-1))*mu)>>(k+1) * q，再 wrap_mod。
 * @param q 模数；@param k/@param mu Barrett 参数
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
 * Stage3 RouteA Horner 合并（无 mod）：((hh<<6)+(hl+lh))<<6 + ll。
 * 平面读数，无 Gather。
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
/** Stage3 默认：Horner + Barrett 向量 mod（见 stage3_mod_variants.hpp）。 */
__aicore__ inline void combine_limb6_routea_mod_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                    LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                    LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                    LocalTensor<int32_t> &t2, int32_t q, int32_t count)
{
    combine_limb6_horner_barrett_vec(dst, hh, lh, hl, ll, t1, t2, q, count);
}
#elif F203_STAGE3_MOD == 1
/** Stage3 对照：标量 i64 mod（调试/非默认生产路径）。 */
__aicore__ inline void combine_limb6_routea_mod_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                    LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                    LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                    LocalTensor<int32_t> &t2, int32_t q, int32_t count)
{
    (void)t2;
    combine_limb6_routea_mod_scalar_i64(dst, hh, lh, hl, ll, t1, q, count);
}
#else
/** Stage3 对照：float Cast+Div mod。 */
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
