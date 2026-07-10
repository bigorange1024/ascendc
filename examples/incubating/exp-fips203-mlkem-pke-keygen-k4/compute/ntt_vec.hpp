// @probe exp-fips203-mlkem-pke-keygen-k4
// @file compute/ntt_vec.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `ntt_vec.hpp` 为该子模块组件。 / Component: ntt_vec.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: basic.hpp, kernel_operator.h, kyber_limb6.hpp, stage1_config.hpp, stage3_config.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 Tag5T NTT 向量原语（S3 merge/mod 等）。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/ntt_vec.hpp
 */
/**
 * @file ntt_vec.hpp
 * @brief F203 Tag5T Stage1 limb6 分裂与 Stage3 RouteA 平面 merge/mod（无 Gather）。
 *
 * 用途：
 *   - Stage1：int32→(hh,hl,lh,ll) 六个 6-bit limb（F203_STAGE1_SPLIT 0/1/2）；
 *   - Stage3：平面读 mat_c 四行 Horner 合并 + mod q（F203_STAGE3_MOD 选变体）。
 *
 * 调用方：`aiv_func.hpp`（Stage1/Pack/Merge）、`2s1e_post_ntt_ub.hpp`（经 hat_vec 间接）、hat_vec.hpp。
 *
 * 不变量：
 *   - kKyberLimbBits=6；RouteA 合并 shift=6；Stage3 内禁止 Gather（项目 ML-KEM 约束）；
 *   - combine_limb6_routea_mod_vec 由 F203_STAGE3_MOD 绑定 stage3_mod_variants.hpp。
 *
 * Golden：gen_data 平面 mat_c + stage31_mod 语义；dst.bin [12,256] NTT 后系数。
 *
 * CMake：F203_STAGE1_SPLIT、F203_STAGE3_MOD（stage1/3_config.hpp；STAGE1 编入 cpu/npu_lib）。
 */
#ifndef __NTT_VEC_HPP__
#define __NTT_VEC_HPP__
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "stage1_config.hpp"
#include "stage3_config.hpp"

struct Tensor_int8x4 {
    LocalTensor<int8_t> &x0;
    LocalTensor<int8_t> &x1;
};

template <typename U, typename T>
__aicore__ static inline auto tr(LocalTensor<T> x)
{
    return x.template ReinterpretCast<U>();
}

__aicore__ static inline void split_2xint6(int8_t &d0, int8_t &d1, int32_t a)
{
    d0 = static_cast<int8_t>(a & kKyberLimbMask);
    d1 = static_cast<int8_t>((a >> kKyberLimbBits) & kKyberLimbMask);
}

/**
 * 本函数为 KeyGen 流水线组件 `split_vec_scalar`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void split_vec_scalar(Tensor_int8x4 &dst, LocalTensor<int32_t> &src, const int32_t count)
{
    auto x0 = tr<int32_t>(dst.x0);
    auto x1 = tr<int32_t>(dst.x1);

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
 * 本函数为 KeyGen 流水线组件 `cast_i32_to_i8`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
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
 * 本函数为 KeyGen 流水线组件 `limb6_tile`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
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
 * 本函数为 KeyGen 流水线组件 `split_vec`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
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
 * 本函数为 KeyGen 流水线组件 `wrap_mod_vec_runtime`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
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

/** Stage3 RouteA 合并与 mod（平面读数，无 Gather）。 */
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
 * 本函数为 KeyGen 流水线组件 `combine_limb6_routea_mod_vec`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void combine_limb6_routea_mod_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                    LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                    LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                    LocalTensor<int32_t> &t2, int32_t q, int32_t count)
{
    combine_limb6_horner_barrett_vec(dst, hh, lh, hl, ll, t1, t2, q, count);
}
#elif F203_STAGE3_MOD == 1
__aicore__ inline void combine_limb6_routea_mod_vec(LocalTensor<int32_t> &dst, LocalTensor<int32_t> &hh,
                                                    LocalTensor<int32_t> &lh, LocalTensor<int32_t> &hl,
                                                    LocalTensor<int32_t> &ll, LocalTensor<int32_t> &t1,
                                                    LocalTensor<int32_t> &t2, int32_t q, int32_t count)
{
    (void)t2;
    combine_limb6_routea_mod_scalar_i64(dst, hh, lh, hl, ll, t1, q, count);
}
#else
/**
 * 本函数为 KeyGen 流水线组件 `combine_limb6_routea_mod_vec`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
 */
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
