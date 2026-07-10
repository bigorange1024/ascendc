/**
 * 【文件头】Alg.11/12 向量实现：SoA lane、Barrett、deinterleave/interleave、B1/B2 分发。
 *
 * 本文件在流水线中的位置：ALG11_IMPL=1 时由 multiply_ntts_ub.hpp 调用的核心计算。
 * 作用：Init ROM → deinterleave 四 lane → 向量 Alg.12 → interleave 回 AoS。
 * 与 golden 关系：I/O 为 [256] int32 AoS 多项式，须与 gen_data 的 golden_h.bin 逐系数一致。
 */
#pragma once

#if ALG11_VEC_OPTS == 1
#include "alg11_fixed_n256.hpp"
#endif
#include "alg11_gammas.h"
#include "alg11_rom_tables.h"
#include "alg11_ub_load.hpp"
#include "alg11_vec_pipe.hpp"
#include "kernel_operator.h"
#include "multiply_ntts_config.hpp"
#include "tiling.h"

namespace alg11_vec {

constexpr int32_t kQ = kAlg11Q;
constexpr int32_t kPairCount = alg11_tiling::kPairCount;
constexpr int32_t kWsLaneCount = 8;
constexpr int32_t kWsLaneCountB2 = 8;

/** 向量工作区：四输入 lane + 两输出 + 两临时；γ/索引可绑 ROM 或 ws 尾部 */
struct VecWs {
    AscendC::LocalTensor<int32_t> a0;
    AscendC::LocalTensor<int32_t> a1;
    AscendC::LocalTensor<int32_t> b0;
    AscendC::LocalTensor<int32_t> b1;
    AscendC::LocalTensor<int32_t> c0;
    AscendC::LocalTensor<int32_t> c1;
    AscendC::LocalTensor<int32_t> t1;
    AscendC::LocalTensor<int32_t> t2;
    AscendC::LocalTensor<int32_t> gammaV;
    AscendC::LocalTensor<int32_t> idx;
    AscendC::LocalTensor<int32_t> idx2;
};

/** Init 物化的 ROM LUT 在 UB 上的视图 */
struct RomUbLuts {
    AscendC::LocalTensor<int32_t> gammaV;
    AscendC::LocalTensor<int32_t> gatherEvenByte;
    AscendC::LocalTensor<int32_t> gatherOddByte;
    AscendC::LocalTensor<int32_t> interleaveReorderByte;
};

/**
 * 将连续 ws 基址切成 a0..t2 等 lane，并绑定 γ/索引。
 * @param base       工作区起点，长度 kVecWsInts
 * @param w          输出 VecWs 视图
 * @param pairCount  每 lane 长度（128）
 * @param rom        ROM LUT（MEM_OPS=1 时 idx 来自 rom）
 * 布局：base[0..7*pair) = a0,a1,b0,b1,c0,c1,t1,t2；legacy 另含 idx/idx2。
 */
__aicore__ inline void bind_vec_ws(AscendC::LocalTensor<int32_t> &base, VecWs &w, int32_t pairCount,
                                   const RomUbLuts &rom)
{
    w.a0 = base[0];
    w.a1 = base[pairCount];
    w.b0 = base[2 * pairCount];
    w.b1 = base[3 * pairCount];
    w.c0 = base[4 * pairCount];
    w.c1 = base[5 * pairCount];
    w.t1 = base[6 * pairCount];
    w.t2 = base[7 * pairCount];
    w.gammaV = rom.gammaV;
#if ALG11_MEM_OPS == 1
    /* 索引在独立 ROM UB，不占 ws */
    w.idx = rom.gatherEvenByte;
    w.idx2 = rom.gatherOddByte;
#else
    /* legacy：索引落在 ws 第 8/9 条 lane */
    w.idx = base[8 * pairCount];
    w.idx2 = base[9 * pairCount];
#endif
}

#if ALG11_MEM_OPS == 1

#if defined(ASCENDC_CPU_DEBUG)
/**
 * tikicpu：ROM DataCopy→Gather 的 MTE2_V 事件易残留；Init 用 SetValue 物化。
 * @param rom,pairCount  同设备路径
 */
__aicore__ inline void init_rom_luts_ub(RomUbLuts &rom, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        rom.gammaV.SetValue(i, kAlg11Gammas[i]);
        rom.gatherEvenByte.SetValue(i, i * 8);
        rom.gatherOddByte.SetValue(i, i * 8 + 4);
    }
}
#else
/**
 * 设备 Init：GM ROM → UB DataCopy（γ + 偶/奇 Gather + interleave 索引）。
 * @param rom        已 Alloc 的 LocalTensor 槽位
 * @param pairCount  128；interleave 拷 pairCount*2
 */
__aicore__ inline void init_rom_luts_ub(RomUbLuts &rom, int32_t pairCount)
{
    alg11_ub_load::copy_rom_int32_ub(rom.gammaV, gAlg11GammasGm, pairCount);
    alg11_ub_load::copy_rom_int32_ub(rom.gatherEvenByte, gAlg11GatherEvenByteGm, pairCount);
    alg11_ub_load::copy_rom_int32_ub(rom.gatherOddByte, gAlg11GatherOddByteGm, pairCount);
    alg11_ub_load::copy_rom_int32_ub(rom.interleaveReorderByte, gAlg11InterleaveReorderByteGm, pairCount * 2);
    ALG11_PIPE_MTE2();
}
#endif

#else

/**
 * legacy：用 SetValue 把 γ 填进 UB（无 GM ROM）。
 * @param gammaV     目标 [pairCount] int32
 * @param pairCount  128
 */
__aicore__ inline void materialize_gamma_lut_ub_once(AscendC::LocalTensor<int32_t> &gammaV, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        gammaV.SetValue(i, kAlg11Gammas[i]);
    }
}

/**
 * legacy Init：仅物化 γ。
 */
__aicore__ inline void init_rom_luts_ub(RomUbLuts &rom, int32_t pairCount)
{
    materialize_gamma_lut_ub_once(rom.gammaV, pairCount);
}

#endif

/**
 * 向量 wrap_mod：x ∈ [0,q) 的末步校正，与 alg11_barrett_red_coeff 末步一致。
 * @param dst,src  输入/输出 LocalTensor（可同址）
 * @param q        模数 3329
 * @param t1,t2    临时 lane
 * @param count    元素个数
 * 语义：dst = src - (q & ~((src-q)>>31)) 的向量化写法（Max 技巧）。
 */
__aicore__ inline void wrap_mod_vec_runtime(AscendC::LocalTensor<int32_t> &dst, AscendC::LocalTensor<int32_t> &src,
                                            int32_t q, AscendC::LocalTensor<int32_t> &t1,
                                            AscendC::LocalTensor<int32_t> &t2, int32_t count)
{
    using AscendC::Adds;
    using AscendC::Max;
    using AscendC::Mul;
    using AscendC::ShiftRight;

    /* t1 = src - q */
    Adds(t1, src, -q, count);
    auto &t1_u32 = *reinterpret_cast<AscendC::LocalTensor<uint32_t> *>(&t1);
    auto &t2_u32 = *reinterpret_cast<AscendC::LocalTensor<uint32_t> *>(&t2);
    /* 算术右移取符号：src≥q 时掩码为 0，否则为全 1 */
    ShiftRight(t2_u32, t1_u32, 31U, count);
    Mul(t2, src, t2, count);
    /* Max(src-q, src&mask) 实现条件减 q */
    Max(dst, t1, t2, count);
}

/**
 * 全向量 Barrett 约化（μ=78,k=18 与 μ=5039,k=24 + wrap_mod）。
 * @param dst  待约化向量（就地）
 * @param s1,s2  临时
 * @param count  元素数
 * 含负值修正（首步按符号抬升），适用于一般中间积。
 */
__aicore__ inline void reduce_zq_vec_barrett(AscendC::LocalTensor<int32_t> &dst, AscendC::LocalTensor<int32_t> &s1,
                                              AscendC::LocalTensor<int32_t> &s2, int32_t count)
{
    using AscendC::Add;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    using AscendC::Sub;

    const int32_t n = count;
    const int32_t q = kQ;

    /* 负值修正：若 dst<0 则加 q */
    ShiftRight(s1, dst, 31, n);
    Muls(s1, s1, -q, n);
    Add(dst, dst, s1, n);
    ALG11_PIPE_ALL();

    /* 第一轮 Barrett */
    Muls(s1, dst, 78, n);
    ShiftRight(s1, s1, 18, n);
    Muls(s1, s1, q, n);
    Sub(dst, dst, s1, n);
    ALG11_PIPE_ALL();

    /* 第二轮 Barrett */
    Muls(s1, dst, 5039, n);
    ShiftRight(s1, s1, 24, n);
    Muls(s1, s1, q, n);
    Sub(dst, dst, s1, n);
    ALG11_PIPE_ALL();

    wrap_mod_vec_runtime(dst, dst, q, s1, s2, n);
}

#if ALG11_VEC_OPTS == 1
/**
 * BaseCaseMultiply / 行 18 专用：输入为 NTT 域 canonical [0,q)，无负值，省略负值修正。
 * @param dst,s1,s2,count  同 reduce_zq_vec_barrett
 */
__aicore__ inline void reduce_zq_vec_barrett_basemul(AscendC::LocalTensor<int32_t> &dst,
                                                    AscendC::LocalTensor<int32_t> &s1,
                                                    AscendC::LocalTensor<int32_t> &s2, int32_t count)
{
    using AscendC::Muls;
    using AscendC::ShiftRight;
    using AscendC::Sub;

    const int32_t n = count;
    const int32_t q = kQ;

    Muls(s1, dst, 78, n);
    ShiftRight(s1, s1, 18, n);
    Muls(s1, s1, q, n);
    Sub(dst, dst, s1, n);
    ALG11_PIPE_ALL();

    Muls(s1, dst, 5039, n);
    ShiftRight(s1, s1, 24, n);
    Muls(s1, s1, q, n);
    Sub(dst, dst, s1, n);
    ALG11_PIPE_ALL();

    wrap_mod_vec_runtime(dst, dst, q, s1, s2, n);
}
#endif

/**
 * 按 ALG11_VEC_OPTS 选择 Barrett 变体。
 */
__aicore__ inline void reduce_zq_vec_barrett_dispatch(AscendC::LocalTensor<int32_t> &dst,
                                                      AscendC::LocalTensor<int32_t> &s1,
                                                      AscendC::LocalTensor<int32_t> &s2, int32_t count)
{
#if ALG11_VEC_OPTS == 1
    reduce_zq_vec_barrett_basemul(dst, s1, s2, count);
#else
    reduce_zq_vec_barrett(dst, s1, s2, count);
#endif
}

#if ALG11_MEM_OPS == 0 || defined(ASCENDC_CPU_DEBUG)

/**
 * 标量 deinterleave：AoS [e0,o0,e1,o1,...] → even/odd 两条 SoA。
 * @param even,odd  输出各 [pairCount]
 * @param aos       输入 [2*pairCount]
 * @param pairCount 对数
 */
__aicore__ inline void deinterleave_pairs_scalar(AscendC::LocalTensor<int32_t> &even,
                                                 AscendC::LocalTensor<int32_t> &odd,
                                                 const AscendC::LocalTensor<int32_t> &aos, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        even.SetValue(i, aos.GetValue(i * 2));
        odd.SetValue(i, aos.GetValue(i * 2 + 1));
    }
}

/**
 * 标量 interleave：even/odd → AoS。
 * @param aos       输出 [2*pairCount]
 * @param even,odd  输入各 [pairCount]
 */
__aicore__ inline void interleave_pairs_scalar(AscendC::LocalTensor<int32_t> &aos, AscendC::LocalTensor<int32_t> &even,
                                               AscendC::LocalTensor<int32_t> &odd, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        aos.SetValue(i * 2, even.GetValue(i));
        aos.SetValue(i * 2 + 1, odd.GetValue(i));
    }
}

#endif

/**
 * B2：4 次宽 Gather 填齐 a0,a1,b0,b1（索引来自 Init DataCopy ROM 或运行时生成）。
 * @param w          工作区（写 a0..b1，读 idx/idx2）
 * @param f,g        AoS 输入 poly [256]
 * @param pairCount  128
 * 分支：MEM_OPS=1 用 ROM 索引；VEC_OPTS=1 用固定公式；否则 CreateVecIndex+Muls+Adds。
 */
__aicore__ inline void deinterleave_four_lanes_gather(VecWs &w, const AscendC::LocalTensor<int32_t> &f,
                                                      const AscendC::LocalTensor<int32_t> &g, int32_t pairCount)
{
    using AscendC::Adds;
    using AscendC::CreateVecIndex;
    using AscendC::Gather;
    using AscendC::Muls;

    const uint32_t n = static_cast<uint32_t>(pairCount);

#if ALG11_MEM_OPS == 1 && !defined(ASCENDC_CPU_DEBUG)
    /* ROM 索引：偶偏移取 a0/b0，奇偏移取 a1/b1 */
    Gather(w.a0, f, w.idx.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.b0, g, w.idx.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.a1, f, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.b1, g, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
#elif ALG11_VEC_OPTS == 1
    /* §9：线性公式填索引后再 Gather */
    alg11_fixed_n256::load_gather_byte_indices(w.idx, w.idx2, pairCount);
    Gather(w.a0, f, w.idx.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.b0, g, w.idx.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.a1, f, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.b1, g, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
#else
    /* legacy：CreateVecIndex(0..) → *8 得偶字节偏移；+4 得奇 */
    CreateVecIndex(w.idx, static_cast<int32_t>(0), n);
    Muls(w.idx2, w.idx, static_cast<int32_t>(8), pairCount);
    Gather(w.a0, f, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.b0, g, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
    Adds(w.idx2, w.idx2, static_cast<int32_t>(4), pairCount);
    Gather(w.a1, f, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.b1, g, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
#endif
}

#if ALG11_MEM_OPS == 1

/**
 * DataCopy+Gather interleave：scratch=t1||c1@t2（ws 内连续 256 int）；reorder 字节索引来自 ROM。
 * @param aos          输出 AoS [256]
 * @param even,odd     c0/c1 SoA [128]
 * @param w            借用 t1/t2 作 scratch
 * @param reorderByte  interleave 字节索引 [256]
 * @param pairCount    128
 */
__aicore__ inline void interleave_pairs_datacopy(AscendC::LocalTensor<int32_t> &aos, AscendC::LocalTensor<int32_t> &even,
                                                 AscendC::LocalTensor<int32_t> &odd, VecWs &w,
                                                 const AscendC::LocalTensor<int32_t> &reorderByte, int32_t pairCount)
{
    using AscendC::DataCopy;
    using AscendC::Gather;

    const uint32_t n = static_cast<uint32_t>(pairCount);
    /* t1||t2 拼成 [c0|c1] 连续 256 int，供 Gather 按字节重排 */
    DataCopy(w.t1, even, n);
    DataCopy(w.t2, odd, n);
    ALG11_PIPE_MTE2();
    Gather(aos, w.t1, reorderByte.ReinterpretCast<uint32_t>(), 0U, n * 2U);
    ALG11_PIPE_ALL();
}

#endif

/**
 * 四 lane 就绪后的向量 Alg.12 主核（B1/B2 共用）；w.gammaV 须 Init 物化。
 * @param w          含 a0..b1、c0/c1、t1/t2、gammaV
 * @param pairCount  128
 * 计算：c0 = a0*b0 + (a1*b1)*γ；c1 = a0*b1 + a1*b0（各步 Barrett）。
 */
__aicore__ inline void alg12_elementwise_vec(VecWs &w, int32_t pairCount)
{
    using AscendC::Add;
    using AscendC::Mul;

    const int32_t n = pairCount;

    /* t1 = reduce(a1*b1)；再 *γ 得 a1b1*γ */
    Mul(w.t1, w.a1, w.b1, n);
    reduce_zq_vec_barrett_dispatch(w.t1, w.t2, w.c0, n);
    Mul(w.t2, w.t1, w.gammaV, n);
    /* c0 = reduce(a0*b0 + a1b1*γ) */
    Mul(w.t1, w.a0, w.b0, n);
    Add(w.c0, w.t1, w.t2, n);
    reduce_zq_vec_barrett_dispatch(w.c0, w.t2, w.t1, n);
    ALG11_PIPE_ALL();

    /* c1 = reduce(a0*b1 + a1*b0) */
    Mul(w.t1, w.a0, w.b1, n);
    Mul(w.t2, w.a1, w.b0, n);
    Add(w.c1, w.t1, w.t2, n);
    reduce_zq_vec_barrett_dispatch(w.c1, w.t2, w.t1, n);
    ALG11_PIPE_ALL();
}

/**
 * B1：deinterleave（CPU/legacy 标量；MEM_OPS=1 设备上走 Gather）。
 * @param w,f,g,pairCount  同 Gather 路径
 */
__aicore__ inline void deinterleave_four_lanes_scalar(VecWs &w, const AscendC::LocalTensor<int32_t> &f,
                                                      const AscendC::LocalTensor<int32_t> &g, int32_t pairCount)
{
#if defined(ASCENDC_CPU_DEBUG)
    deinterleave_pairs_scalar(w.a0, w.a1, f, pairCount);
    deinterleave_pairs_scalar(w.b0, w.b1, g, pairCount);
#elif ALG11_MEM_OPS == 1
    deinterleave_four_lanes_gather(w, f, g, pairCount);
#else
    deinterleave_pairs_scalar(w.a0, w.a1, f, pairCount);
    deinterleave_pairs_scalar(w.b0, w.b1, g, pairCount);
#endif
}

/**
 * interleave 分发：MEM_OPS=1 设备用 DataCopy+Gather；否则标量。
 * @param h    输出 AoS
 * @param w    含 c0/c1
 * @param rom  interleave 索引
 */
__aicore__ inline void interleave_pairs_dispatch(AscendC::LocalTensor<int32_t> &h, VecWs &w,
                                                 const RomUbLuts &rom, int32_t pairCount)
{
#if ALG11_MEM_OPS == 1
#if defined(ASCENDC_CPU_DEBUG)
    interleave_pairs_scalar(h, w.c0, w.c1, pairCount);
#else
    interleave_pairs_datacopy(h, w.c0, w.c1, w, rom.interleaveReorderByte, pairCount);
#endif
#else
    interleave_pairs_scalar(h, w.c0, w.c1, pairCount);
#endif
}

/**
 * B1 全路径：deinterleave → Alg.12 向量 → interleave。
 * @param h,f,g  AoS [256] int32
 * @param w,rom  工作区与 ROM
 * @param pairCount  128
 */
__aicore__ inline void multiply_ntts_vec_b1(AscendC::LocalTensor<int32_t> &h, const AscendC::LocalTensor<int32_t> &f,
                                            const AscendC::LocalTensor<int32_t> &g, VecWs &w, const RomUbLuts &rom,
                                            int32_t pairCount)
{
    deinterleave_four_lanes_scalar(w, f, g, pairCount);
    ALG11_PIPE_ALL();
    alg12_elementwise_vec(w, pairCount);
    interleave_pairs_dispatch(h, w, rom, pairCount);
    ALG11_PIPE_ALL();
}

/**
 * B2 全路径：Gather deinterleave（CPU 回退标量）→ Alg.12 → interleave。
 */
__aicore__ inline void multiply_ntts_vec_b2(AscendC::LocalTensor<int32_t> &h, const AscendC::LocalTensor<int32_t> &f,
                                            const AscendC::LocalTensor<int32_t> &g, VecWs &w, const RomUbLuts &rom,
                                            int32_t pairCount)
{
#if defined(ASCENDC_CPU_DEBUG)
    deinterleave_four_lanes_scalar(w, f, g, pairCount);
#else
    deinterleave_four_lanes_gather(w, f, g, pairCount);
#endif
    ALG11_PIPE_ALL();
    alg12_elementwise_vec(w, pairCount);
    interleave_pairs_dispatch(h, w, rom, pairCount);
    ALG11_PIPE_ALL();
}

/**
 * 按 ALG11_VEC_VARIANT 选择 B1/B2（默认 B2）。
 */
__aicore__ inline void multiply_ntts_vec_dispatch(AscendC::LocalTensor<int32_t> &h, const AscendC::LocalTensor<int32_t> &f,
                                                const AscendC::LocalTensor<int32_t> &g, VecWs &w, const RomUbLuts &rom,
                                                int32_t pairCount)
{
#if ALG11_VEC_VARIANT == 2
    multiply_ntts_vec_b2(h, f, g, w, rom, pairCount);
#elif ALG11_VEC_VARIANT == 1
    multiply_ntts_vec_b1(h, f, g, w, rom, pairCount);
#else
    multiply_ntts_vec_b2(h, f, g, w, rom, pairCount);
#endif
}

}  // namespace alg11_vec
