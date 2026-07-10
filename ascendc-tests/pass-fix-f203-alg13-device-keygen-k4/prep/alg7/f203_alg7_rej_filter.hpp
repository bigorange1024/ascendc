// @probe pass-fix-f203-alg13-device-keygen-k4
// @file prep/alg7/f203_alg7_rej_filter.hpp
// @layer prep
// @role prep/alg7：FIPS203 Algorithm 7 SampleNTT（CBD+rej 采样 ŝ/ê 等）；含 XOF、rej 标量/向量与 compact LUT ROM。 / Alg.7 SampleNTT rejection-sampling prep kernels. 本文件 `f203_alg7_rej_filter.hpp` 为该子模块组件。 / Component: f203_alg7_rej_filter.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_alg7_config.h, f203_alg7_layout.h, kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file f203_alg7_rej_filter.hpp
 * @brief Alg.7 rej「剔除」段：向量标记 d≥q 为拒绝（lane 置 q），**热路径禁止 for+GetValue**。
 *
 * 流水线位置：rej_vec 在交错前对 d1Work/d2Work 各调用一次 RejectFilterDispatchUb。
 *
 * 实现分支（F203_ALG7_REJ_IMPL）：
 *   1 — Mins(d,q)：接受保留 d∈[0,q-1]，拒绝 lane 变为 q（**生产默认**）
 *   2 — Compares(LT,q) + Select(q,d)：128-lane tile，实验对照
 *
 * CPU 孪生：IMPL=2 在 ASCENDC_CPU_DEBUG 下 dispatch 回退 Mins（tikicpulib 无 int32 Select 桩）。
 *
 * 与 golden 关系：剔除后 stream 上拒绝位置为 q，compact 段跳过 v==q。
 */
#pragma once

#include "f203_alg7_config.h"
#include "f203_alg7_layout.h"

#include "kernel_operator.h"

namespace F203Alg7 {

#define F203_ALG7_FILTER_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()

/**
 * 方案 B（IMPL=1，生产）：Mins(dst, src, q) 逐 lane 取 min。
 * 语义：若 src<q 则 dst=src；若 src≥q 则 dst=q（拒绝标记）。
 *
 * @param dst   输出 UB int32[count]，可与 src 同址 in-place
 * @param src   输入 UB int32[count]
 * @param count 元素个数（224）
 */
__aicore__ inline void RejectFilterMinsUb(AscendC::LocalTensor<int32_t> &dst, const AscendC::LocalTensor<int32_t> &src,
                                          uint32_t count)
{
    using AscendC::Mins;
    const int32_t n = static_cast<int32_t>(count);
    const int32_t q = static_cast<int32_t>(kKyberQ);
    Mins(dst, src, q, n);
    F203_ALG7_FILTER_PIPE_ALL();
}

#if F203_ALG7_REJ_IMPL == F203_ALG7_REJ_VEC_MASK && !defined(ASCENDC_CPU_DEBUG)

/**
 * 单 tile（128 int32）：Compares(LT,q) 得 uint8 掩码；Select 按掩码在 src 与 q 间选择。
 * mask=1（接受）取 src；mask=0（拒绝）取 q。
 */
__aicore__ inline void RejectFilterMaskLtTile128Ub(AscendC::LocalTensor<int32_t> &dst,
                                                   const AscendC::LocalTensor<int32_t> &src,
                                                   AscendC::LocalTensor<uint8_t> &cmpMaskUb,
                                                   const AscendC::LocalTensor<int32_t> &qFillUb)
{
    using AscendC::CMPMODE;
    using AscendC::Compares;
    using AscendC::SELMODE;
    using AscendC::Select;

    const int32_t q = static_cast<int32_t>(kKyberQ);
    const uint32_t n = kRejectFilterTileInt32;
    Compares(cmpMaskUb, src, q, CMPMODE::LT, n);
    F203_ALG7_FILTER_PIPE_ALL();
    Select(dst, cmpMaskUb, qFillUb, src, SELMODE::VSEL_TENSOR_TENSOR_MODE, n);
    F203_ALG7_FILTER_PIPE_ALL();
}

/**
 * 方案 A（IMPL=2）：kCandPairs=224 = 128 + 96，第二 tile 尾部 96 有效 + 32 pad 至 128。
 * tileUb 复用：先 Duplicate 全 q 作 qFill，再对尾段 DataCopy+Duplicate pad。
 */
__aicore__ inline void RejectFilterMaskLtUb(AscendC::LocalTensor<int32_t> &dst, const AscendC::LocalTensor<int32_t> &src,
                                            AscendC::LocalTensor<uint8_t> &cmpMaskUb,
                                            AscendC::LocalTensor<int32_t> &tileUb)
{
    using AscendC::DataCopy;
    using AscendC::Duplicate;

    const int32_t q = static_cast<int32_t>(kKyberQ);
    Duplicate(tileUb, q, kRejectFilterTileInt32);
    F203_ALG7_FILTER_PIPE_ALL();

    // 第一 tile：src[0..127]
    RejectFilterMaskLtTile128Ub(dst, src, cmpMaskUb, tileUb);

    // 第二 tile：src[128..223]，不足 128 的 lane 用 q 填充（Compares 对齐要求）
    constexpr uint32_t kTailValid = kCandPairs - kRejectFilterTileInt32;  // 96
    constexpr uint32_t kTailPad = kRejectFilterTileInt32 - kTailValid;      // 32
    DataCopy(tileUb, src[kRejectFilterTileInt32], kTailValid);
    Duplicate(tileUb[kTailValid], q, kTailPad);
    F203_ALG7_FILTER_PIPE_ALL();
    RejectFilterMaskLtTile128Ub(dst[kRejectFilterTileInt32], tileUb, cmpMaskUb, tileUb);
}

#endif  // IMPL==2 && !CPU

/**
 * 剔除实现分发：IMPL=2 且非 CPU 用 Mask；否则 Mins（含 CPU 孪生回退）。
 */
__aicore__ inline void RejectFilterDispatchUb(AscendC::LocalTensor<int32_t> &dst, const AscendC::LocalTensor<int32_t> &src,
                                            AscendC::LocalTensor<uint8_t> &cmpMaskUb,
                                            AscendC::LocalTensor<int32_t> &tileUb)
{
#if F203_ALG7_REJ_IMPL == F203_ALG7_REJ_VEC_MASK && !defined(ASCENDC_CPU_DEBUG)
    RejectFilterMaskLtUb(dst, src, cmpMaskUb, tileUb);
#else
    (void)cmpMaskUb;
    (void)tileUb;
    RejectFilterMinsUb(dst, src, kCandPairs);
#endif
}

}  // namespace F203Alg7
