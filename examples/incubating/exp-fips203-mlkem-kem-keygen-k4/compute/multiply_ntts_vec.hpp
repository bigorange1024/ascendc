// @probe stable-fips203-mlkem-pke-keygen-k4
// @file compute/multiply_ntts_vec.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `multiply_ntts_vec.hpp` 为该子模块组件。 / Component: multiply_ntts_vec.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: alg11_fixed_n256.hpp, alg11_gammas.h, alg11_rom_tables.h, alg11_ub_load.hpp, alg11_vec_pipe.hpp, kernel_operator.h, multiply_ntts_config.hpp, alg11_tiling.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 NTT 后乘积 / 配置辅助。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/multiply_ntts_vec.hpp
 */
/**
 * @file multiply_ntts_vec.hpp
 * @brief Alg.11 MultiplyNTTs 向量实现：SoA 车道、Gather 解交错（B2）、Barrett basemul、interleave 写回。
 *
 * 用途：ALG11_IMPL=1 时设备/SIM 热路径；含 VecWs/RomUbLuts 绑定、init_rom_luts_ub、multiply_ntts_vec_dispatch。
 *
 * 调用方：`multiply_ntts_ub.hpp`、`hat_alg11_basemul.hpp`；不直接被 main 引用。
 *
 * 不变量：
 *   - 128 对 (a0,a1)×(b0,b1)，γ[i]=ζ^(2·BitRev7(i)+1) mod q；
 *   - ALG11_VEC_VARIANT=2 用共享 Gather 索引（gatherEven/OddByte GM→UB）；
 *   - ALG11_VEC_OPTS=1 用 alg11_fixed_n256 线性索引 + reduce_zq_vec_barrett_basemul。
 *
 * Golden：数学对齐 hat_inner_product_ref.c::hat_multiply_ntts；验收为行 18 整多项式 I/O。
 *
 * CMake：ALG11_IMPL、ALG11_VEC_VARIANT、ALG11_VEC_OPTS、ALG11_MEM_OPS。
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
#include "alg11_tiling.h"

namespace alg11_vec {

constexpr int32_t kQ = kAlg11Q;
constexpr int32_t kPairCount = alg11_tiling::kPairCount;
constexpr int32_t kWsLaneCount = 8;
constexpr int32_t kWsLaneCountB2 = 8;

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

struct RomUbLuts {
    AscendC::LocalTensor<int32_t> gammaV;
    AscendC::LocalTensor<int32_t> gatherEvenByte;
    AscendC::LocalTensor<int32_t> gatherOddByte;
    AscendC::LocalTensor<int32_t> interleaveReorderByte;
};

/**
 * 本函数为 KeyGen 流水线组件 `bind_vec_ws`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
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
    w.idx = rom.gatherEvenByte;
    w.idx2 = rom.gatherOddByte;
#else
    w.idx = base[8 * pairCount];
    w.idx2 = base[9 * pairCount];
#endif
}

#if ALG11_MEM_OPS == 1

/**
 * 本函数为 KeyGen 流水线组件 `init_rom_luts_ub`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void init_rom_luts_ub(RomUbLuts &rom, int32_t pairCount)
{
    alg11_ub_load::copy_rom_int32_ub(rom.gammaV, gAlg11GammasGm, pairCount);
    ALG11_PIPE_ALL();
    alg11_ub_load::copy_rom_int32_ub(rom.gatherEvenByte, gAlg11GatherEvenByteGm, pairCount);
    alg11_ub_load::copy_rom_int32_ub(rom.gatherOddByte, gAlg11GatherOddByteGm, pairCount);
    alg11_ub_load::copy_rom_int32_ub(rom.interleaveReorderByte, gAlg11InterleaveReorderByteGm, pairCount * 2);
    alg11_mte2_to_v_sync();
}

#else

__aicore__ inline void materialize_gamma_lut_ub_once(AscendC::LocalTensor<int32_t> &gammaV, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        gammaV.SetValue(i, kAlg11Gammas[i]);
    }
}

/**
 * 本函数为 KeyGen 流水线组件 `init_rom_luts_ub`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void init_rom_luts_ub(RomUbLuts &rom, int32_t pairCount)
{
    materialize_gamma_lut_ub_once(rom.gammaV, pairCount);
}

#endif

/** x ∈ [0,q)：x - (q & ~((x-q)>>31))，与 alg11_barrett_red_coeff 末步一致。 */
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

/** 全向量 Barrett 约化（μ=78,k=18 与 μ=5039,k=24 + wrap_mod）。 */
__aicore__ inline void reduce_zq_vec_barrett(AscendC::LocalTensor<int32_t> &dst, AscendC::LocalTensor<int32_t> &s1,
                                              AscendC::LocalTensor<int32_t> &s2, int32_t count)
{
    using AscendC::Add;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    using AscendC::Sub;

    const int32_t n = count;
    const int32_t q = kQ;

    ShiftRight(s1, dst, 31, n);
    Muls(s1, s1, -q, n);
    Add(dst, dst, s1, n);
    ALG11_PIPE_ALL();

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

#if ALG11_VEC_OPTS == 1
/**
 * BaseCaseMultiply / 行 18 专用：输入为 NTT 域 canonical [0,q)，无负值，省略负值修正。
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
 * 本函数为 KeyGen 流水线组件 `reduce_zq_vec_barrett_dispatch`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
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

__aicore__ inline void deinterleave_pairs_scalar(AscendC::LocalTensor<int32_t> &even,
                                                 AscendC::LocalTensor<int32_t> &odd,
                                                 const AscendC::LocalTensor<int32_t> &aos, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        even.SetValue(i, aos.GetValue(i * 2));
        odd.SetValue(i, aos.GetValue(i * 2 + 1));
    }
}

/** 半多项式（pairCount<128）写回；ROM reorder 固定 n=256（c1@+512B）。 */
__aicore__ inline void interleave_pairs_scalar(AscendC::LocalTensor<int32_t> &aos, AscendC::LocalTensor<int32_t> &even,
                                               AscendC::LocalTensor<int32_t> &odd, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        aos.SetValue(i * 2, even.GetValue(i));
        aos.SetValue(i * 2 + 1, odd.GetValue(i));
    }
}

/** B2：4 次宽 Gather 填齐 a0,a1,b0,b1（索引来自 Init DataCopy ROM）。 */
__aicore__ inline void deinterleave_four_lanes_gather(VecWs &w, const AscendC::LocalTensor<int32_t> &f,
                                                      const AscendC::LocalTensor<int32_t> &g, int32_t pairCount)
{
    using AscendC::Adds;
    using AscendC::CreateVecIndex;
    using AscendC::Gather;
    using AscendC::Muls;

    const uint32_t n = static_cast<uint32_t>(pairCount);

#if ALG11_MEM_OPS == 1
    Gather(w.a0, f, w.idx.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.b0, g, w.idx.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.a1, f, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.b1, g, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
#elif ALG11_VEC_OPTS == 1
    alg11_fixed_n256::load_gather_byte_indices(w.idx, w.idx2, pairCount);
    Gather(w.a0, f, w.idx.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.b0, g, w.idx.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.a1, f, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.b1, g, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
#else
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

/** scratch=t1||c1@t2（ws 内连续 256 int）；reorder 字节索引来自 ROM。 */
__aicore__ inline void interleave_pairs_datacopy(AscendC::LocalTensor<int32_t> &aos, AscendC::LocalTensor<int32_t> &even,
                                                 AscendC::LocalTensor<int32_t> &odd, VecWs &w,
                                                 const AscendC::LocalTensor<int32_t> &reorderByte, int32_t pairCount)
{
    using AscendC::DataCopy;
    using AscendC::Gather;

    const uint32_t n = static_cast<uint32_t>(pairCount);
    DataCopy(w.t1, even, n);
    DataCopy(w.t2, odd, n);
    alg11_mte2_to_v_sync();
    Gather(aos, w.t1, reorderByte.ReinterpretCast<uint32_t>(), 0U, n * 2U);
    ALG11_PIPE_ALL();
}

#endif

/** 四 lane 就绪后的向量 Alg.12 主核（B1/B2 共用）；w.gammaV 须 Init 物化。 */
__aicore__ inline void alg12_elementwise_vec(VecWs &w, int32_t pairCount)
{
    using AscendC::Add;
    using AscendC::Mul;

    const int32_t n = pairCount;

    Mul(w.t1, w.a1, w.b1, n);
    reduce_zq_vec_barrett_dispatch(w.t1, w.t2, w.c0, n);
    Mul(w.t2, w.t1, w.gammaV, n);
    Mul(w.t1, w.a0, w.b0, n);
    Add(w.c0, w.t1, w.t2, n);
    reduce_zq_vec_barrett_dispatch(w.c0, w.t2, w.t1, n);
    ALG11_PIPE_ALL();

    Mul(w.t1, w.a0, w.b1, n);
    Mul(w.t2, w.a1, w.b0, n);
    Add(w.c1, w.t1, w.t2, n);
    reduce_zq_vec_barrett_dispatch(w.c1, w.t2, w.t1, n);
    ALG11_PIPE_ALL();
}

/** B1：Gather deinterleave（与 B2 共用 ROM 索引）。 */
__aicore__ inline void deinterleave_four_lanes_scalar(VecWs &w, const AscendC::LocalTensor<int32_t> &f,
                                                      const AscendC::LocalTensor<int32_t> &g, int32_t pairCount)
{
#if ALG11_MEM_OPS == 1
    deinterleave_four_lanes_gather(w, f, g, pairCount);
#else
    deinterleave_pairs_scalar(w.a0, w.a1, f, pairCount);
    deinterleave_pairs_scalar(w.b0, w.b1, g, pairCount);
#endif
}

__aicore__ inline void interleave_pairs_dispatch(AscendC::LocalTensor<int32_t> &h, VecWs &w,
                                                 const RomUbLuts &rom, int32_t pairCount)
{
#if ALG11_MEM_OPS == 1
    if (pairCount == alg11_tiling::kPairCount) {
        interleave_pairs_datacopy(h, w.c0, w.c1, w, rom.interleaveReorderByte, pairCount);
    } else {
        interleave_pairs_scalar(h, w.c0, w.c1, pairCount);
    }
#else
    interleave_pairs_scalar(h, w.c0, w.c1, pairCount);
#endif
}

/**
 * 本函数为 KeyGen 流水线组件 `multiply_ntts_vec_b1`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
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

__aicore__ inline void multiply_ntts_vec_b2(AscendC::LocalTensor<int32_t> &h, const AscendC::LocalTensor<int32_t> &f,
                                            const AscendC::LocalTensor<int32_t> &g, VecWs &w, const RomUbLuts &rom,
                                            int32_t pairCount)
{
    deinterleave_four_lanes_gather(w, f, g, pairCount);
    ALG11_PIPE_ALL();
    alg12_elementwise_vec(w, pairCount);
    interleave_pairs_dispatch(h, w, rom, pairCount);
    ALG11_PIPE_ALL();
}

/**
 * 本函数为 KeyGen 流水线组件 `multiply_ntts_vec_dispatch`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 golden 仅 I/O 等价。
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
