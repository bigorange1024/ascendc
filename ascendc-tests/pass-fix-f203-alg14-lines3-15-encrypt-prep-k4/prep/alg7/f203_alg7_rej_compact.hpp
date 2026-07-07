// @probe stable-mlkem-f203-pke-keygen-k4
// @file prep/alg7/f203_alg7_rej_compact.hpp
// @layer prep
// @role prep/alg7：FIPS203 Algorithm 7 SampleNTT（CBD+rej 采样 ŝ/ê 等）；含 XOF、rej 标量/向量与 compact LUT ROM。 / Alg.7 SampleNTT rejection-sampling prep kernels. 本文件 `f203_alg7_rej_compact.hpp` 为该子模块组件。 / Component: f203_alg7_rej_compact.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_alg7_compact_lut.h, f203_alg7_layout.h, f203_alg7_rej_scalar.hpp, kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file f203_alg7_rej_compact.hpp
 * @brief Alg.7 rej compact：从交错 stream[448] 按序取前 256 个接受系数（v<q）。
 *
 * 流水线位置：向量 rej 的 compact 段；当前生产路径由 rej_vec 调用标量 fallback，
 * 本文件提供 R5 向量 Compare(EQ)+LUT Gather 实现供 SIM/NPU 实验。
 *
 * 向量路径（A2 合规）：
 *   - 每 8-lane chunk：Compare(tile, qTile, EQ) 得拒绝掩码
 *   - 掩码取反 → 8-bit accept mask → LUT 查 nTake 与 Gather 字节偏移
 *   - int32 上 Compare 仅 EQ 合法（非 Compares 标量版）
 *
 * CPU 孪生：ASCENDC_CPU_DEBUG 下 RejCompactDispatchUb 回退 RejScalarCompactStreamUb。
 *
 * 与 golden 关系：向量 compact 启用时须与标量 compact  bit-exact。
 */
#pragma once

#include "f203_alg7_compact_lut.h"
#include "f203_alg7_layout.h"
#include "f203_alg7_rej_scalar.hpp"

#include "kernel_operator.h"

namespace F203Alg7 {

#define F203_ALG7_COMPACT_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()

/**
 * 将 Compare(EQ) 输出的拒绝掩码字节转为 8-bit accept mask。
 * Compare：bit=1 表示 lane==q（拒绝）；compact 需要接受掩码 → 低 8 bit 取反。
 */
__aicore__ inline uint8_t PackAcceptMask8FromCmpEqUb(const AscendC::LocalTensor<uint8_t> &cmpMaskUb)
{
    const uint8_t rejectByte = cmpMaskUb.GetValue(0);
    return static_cast<uint8_t>((~rejectByte) & 0xFFu);
}

#if !defined(ASCENDC_CPU_DEBUG)

/**
 * 向量 compact 主循环：按 8-lane chunk 扫描 stream，累积写入 aOut 直至 256。
 *
 * @param stream   交错 stream[448]，拒绝 lane 已标 q
 * @param tileUb   8-lane 工作区 + Compare pad 至 128
 * @param cmpMaskUb Compare 输出掩码 UB
 * @param qTile    128 int32 全 q，与 tileUb Compare EQ
 * @param idxUb    Gather 索引临时（每 chunk 最多 8 个接受）
 * @param outSlice Gather 输出切片
 * @param aOut     输出 â，累积写入
 * @return         写入系数总数
 */
__aicore__ inline uint32_t RejVecCompactStreamUb(const AscendC::LocalTensor<int32_t> &stream,
                                                 AscendC::LocalTensor<int32_t> &tileUb,
                                                 AscendC::LocalTensor<uint8_t> &cmpMaskUb,
                                                 AscendC::LocalTensor<int32_t> &qTile,
                                                 AscendC::LocalTensor<int32_t> &idxUb,
                                                 AscendC::LocalTensor<int32_t> &outSlice,
                                                 AscendC::LocalTensor<int32_t> &aOut)
{
    using AscendC::CMPMODE;
    using AscendC::Compare;
    using AscendC::DataCopy;
    using AscendC::Duplicate;
    using AscendC::Gather;

    const int32_t q = static_cast<int32_t>(kKyberQ);
    uint32_t acc = 0U;

    // Compare 要求 count=128：前 8 lane 为数据，后 120 lane pad 为 q
    Duplicate(qTile, q, kAlg7CompactCompareCount);
    Duplicate(tileUb[kAlg7CompactChunkLanes], q, kAlg7CompactComparePad);
    F203_ALG7_COMPACT_PIPE_ALL();

    for (uint32_t chunk = 0U; chunk < kAlg7CompactStreamChunks && acc < kKyberN; ++chunk) {
        const uint32_t off = chunk * kAlg7CompactChunkLanes;
        DataCopy(tileUb, stream[off], kAlg7CompactChunkLanes);
        F203_ALG7_COMPACT_PIPE_ALL();
        Compare(cmpMaskUb, tileUb, qTile, CMPMODE::EQ, kAlg7CompactCompareCount);
        F203_ALG7_COMPACT_PIPE_ALL();

        const uint8_t accept8 = PackAcceptMask8FromCmpEqUb(cmpMaskUb);
        uint32_t nTake = static_cast<uint32_t>(kAlg7CompactMask8Count[accept8]);
        if (nTake == 0U) {
            continue;  // 本 chunk 无接受系数
        }
        if (acc + nTake > kKyberN) {
            nTake = kKyberN - acc;  // 末 chunk 可能只需部分系数
        }

        // LUT 提供接受 lane 在 8×int32 tile 内的字节偏移
        for (uint32_t t = 0U; t < nTake; ++t) {
            idxUb.SetValue(t, kAlg7CompactMask8GatherByte[accept8][t]);
        }
        F203_ALG7_COMPACT_PIPE_ALL();
        Gather(outSlice, tileUb, idxUb.ReinterpretCast<uint32_t>(), 0U, nTake);
        F203_ALG7_COMPACT_PIPE_ALL();
        DataCopy(aOut[acc], outSlice, nTake);
        F203_ALG7_COMPACT_PIPE_ALL();
        acc += nTake;
    }
    return acc;
}

#endif  // !ASCENDC_CPU_DEBUG

/**
 * compact 分发：SIM/NPU 走向量；CPU 孪生走标量（避免 Compare 掩码语义差异）。
 */
__aicore__ inline uint32_t RejCompactDispatchUb(const AscendC::LocalTensor<int32_t> &stream,
                                                AscendC::LocalTensor<int32_t> &tileUb,
                                                AscendC::LocalTensor<uint8_t> &cmpMaskUb,
                                                AscendC::LocalTensor<int32_t> &qTile,
                                                AscendC::LocalTensor<int32_t> &idxUb,
                                                AscendC::LocalTensor<int32_t> &outSlice,
                                                AscendC::LocalTensor<int32_t> &aOut)
{
#if defined(ASCENDC_CPU_DEBUG)
    (void)tileUb;
    (void)cmpMaskUb;
    (void)qTile;
    (void)idxUb;
    (void)outSlice;
    return RejScalarCompactStreamUb(stream, kStreamLen, aOut);
#else
    return RejVecCompactStreamUb(stream, tileUb, cmpMaskUb, qTile, idxUb, outSlice, aOut);
#endif
}

}  // namespace F203Alg7
