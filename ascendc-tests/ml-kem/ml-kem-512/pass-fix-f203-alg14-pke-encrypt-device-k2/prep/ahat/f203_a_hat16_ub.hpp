// @probe pass-fix-f203-alg14-pke-encrypt-device-k2
// @file prep/ahat/f203_a_hat16_ub.hpp
// @layer prep
// @role prep/ahat：设备侧生成 Encrypt 用矩阵 A_hat（FIPS203 Alg.14 行 3–7）；AIV-only UB 流水，为 compute MMAD 提供 a_hat GM。
// @production_io 默认 run.sh 生产 I/O：input/ek_pke.bin(800B)+m.bin+coins.bin；output/c.bin(768B)；a_hat/re 仅为 device arena 中间态。
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_a_hat16_config.h, f203_a_hat16_layout.h, f203_alg7_d12_vec.hpp, f203_alg7_g.hpp, f203_alg7_layout.h, f203_alg7_shake_xof.hpp, shake_general.h, shake_general_tiling_data.h, shake_ub_helpers.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 1 Â[4,256] 分片构建。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-512（k=2）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：prep/ahat/f203_a_hat16_ub.hpp
 */
/**
 * @file f203_a_hat16_ub.hpp
 * @brief Alg.14 行 3–7：4×（SHAKE + 向量 d12/rej），UB 全链，链末写 GM a_hat[4,256]。
 *
 * 复用 pass-fix-f203-alg7-sample-ntt-k2 的 F203Alg7 模块（Mins+Gather+标量 compact）。
 * 每 poly：SHAKE → 解交织 → d12 → rej → DataCopy 到 a_hat_offset(p,j)。
 */
#pragma once

#include "f203_a_hat16_config.h"
#include "f203_a_hat16_layout.h"
#include "f203_alg7_d12_vec.hpp"
#include "f203_alg7_g.hpp"
#include "f203_alg7_layout.h"
#include "f203_alg7_shake_xof.hpp"
#include "shake_general.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"

namespace F203Ahat16 {

#if F203_AHAT16_BLOCK_DIM == 2
/** D14 锁定：Â[4] 由双 AIV 按 2+2 分片，禁止补到 6/8/16。 */
constexpr uint32_t kShardPolysAiv0 = 2U;
constexpr uint32_t kShardPolysAiv1 = kShakeBatch - kShardPolysAiv0;
constexpr uint32_t kShardPolysMax = kShardPolysAiv0;
#else
constexpr uint32_t kShardPolysMax = kShakeBatch;
#endif

#define F203_AHAT16_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()

/** 设备侧 GM 偏移（与 layout.h AHatOffset 公式一致）。 */
__aicore__ inline uint32_t AHatOffsetUb(uint32_t p, uint32_t j)
{
    return (p * kKyberK + j) * kKyberN;
}

__aicore__ inline void PolyIdxToPJUb(uint32_t polyIdx, uint8_t &p, uint8_t &j)
{
    p = static_cast<uint8_t>(polyIdx / kKyberK);
    j = static_cast<uint8_t>(polyIdx % kKyberK);
}

/** 从 batch yUb 取第 localRow 行 672B 到 xofUb（localRow 为分片内 0..polyCount-1）。 */
__aicore__ inline void CopyXofRowFromBatchUb(const AscendC::LocalTensor<uint8_t> &yUb, uint32_t localRow,
                                             AscendC::LocalTensor<uint8_t> &xofUb)
{
    const uint32_t base = localRow * F203Alg7::kXofBytes;
    for (uint32_t i = 0U; i < F203Alg7::kXofBytes; ++i) {
        xofUb.SetValue(i, yUb.GetValue(base + i));
    }
    F203_AHAT16_PIPE_ALL();
}
/** batch x 行 stride：须 32B 对齐，供 KernelShakeGeneral 内 uint64 块读（34B 有效长仍写入 lengths）。 */
constexpr uint32_t kShakeMsgStride = ShakeXofUb::CeilAlign32(F203Alg7::kSampleSeedBytes);
constexpr uint32_t kShakeActiveBatch = kShardPolysMax;
constexpr uint32_t kShakeBatchXUbBytes = ShakeXofUb::CeilAlign32(kShakeActiveBatch * kShakeMsgStride);
constexpr uint32_t kShakeBatchLenUbBytes =
    ShakeXofUb::CeilAlign32(kShakeActiveBatch * static_cast<uint32_t>(sizeof(uint32_t)));
constexpr uint32_t kShakeBatchYUbBytes = ShakeXofUb::CeilAlign32(kShakeActiveBatch * F203Alg7::kXofBytes);

/** 填 batch 消息：全局 polyIdx ∈ [polyBegin, polyBegin+polyCount)；UB 行下标 0..polyCount-1。 */
__aicore__ inline void FillBatchSampleSeedsRangeUb(const uint8_t rho[F203Alg7::kRhoBytes],
                                                   AscendC::LocalTensor<uint8_t> &xUb,
                                                   AscendC::LocalTensor<uint32_t> &lengthsUb, uint32_t polyBegin,
                                                   uint32_t polyCount)
{
    for (uint32_t localRow = 0U; localRow < polyCount; ++localRow) {
        const uint32_t polyIdx = polyBegin + localRow;
        uint8_t p = 0U;
        uint8_t j = 0U;
        PolyIdxToPJUb(polyIdx, p, j);
        const uint32_t rowBase = localRow * kShakeMsgStride;
        ShakeXofUb::FillShakeRowUb(rho, F203Alg7::kRhoBytes, j, xUb, rowBase);
        xUb.SetValue(rowBase + 33U, p);
        lengthsUb.SetValue(localRow, F203Alg7::kSampleSeedBytes);
    }
    F203_AHAT16_PIPE_ALL();
}

/** 填 batch=4（或 2 AIV 时最大 batch=2）全量消息；等价于 range(0, kShakeActiveBatch)。 */
__aicore__ inline void FillBatchSampleSeedsUb(const uint8_t rho[F203Alg7::kRhoBytes],
                                              AscendC::LocalTensor<uint8_t> &xUb,
                                              AscendC::LocalTensor<uint32_t> &lengthsUb)
{
    FillBatchSampleSeedsRangeUb(rho, xUb, lengthsUb, 0U, kShakeActiveBatch);
}

/** 单 poly：xofUb[672] 已在 UB → d12 → 向量 rej → aLocal[256]。rejWs.idxRom 须在循环外已 Init。 */
__aicore__ inline void SampleNttVecOnePolyFromXofUb(const AscendC::LocalTensor<uint8_t> &xofUb,
                                                    AscendC::TBuf<AscendC::TPosition::VECCALC> &scratchBuf,
                                                    AscendC::TQue<AscendC::TPosition::VECOUT, 1> &d1Que,
                                                    AscendC::TQue<AscendC::TPosition::VECOUT, 1> &d2Que,
                                                    AscendC::TQue<AscendC::TPosition::VECOUT, 1> &aHatQue,
                                                    F203Alg7::Alg7RejVecWs &rejWs,
                                                    AscendC::LocalTensor<int32_t> &aLocal)
{
    using F203Alg7::Alg7D12VecWs;
    using F203Alg7::BindAlg7D12Ws;
    using F203Alg7::BindAlg7RejVecWs;
    using F203Alg7::ComputeD12Vec;
    using F203Alg7::DeinterleaveCandFromUb;
    using F203Alg7::RejVecBulkFromD12Ub;
    using F203Alg7::RejScalarFromD12Ub;
    using F203Alg7::kD12WsInt32Active;

    AscendC::LocalTensor<uint8_t> scratchU8 = scratchBuf.Get<uint8_t>();
    Alg7D12VecWs ws{};
    BindAlg7D12Ws(scratchU8, ws);
    AscendC::LocalTensor<int32_t> expandedUnused = scratchU8[0].ReinterpretCast<int32_t>();
    DeinterleaveCandFromUb(xofUb, ws, expandedUnused);

    AscendC::LocalTensor<int32_t> d1Local = d1Que.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> d2Local = d2Que.AllocTensor<int32_t>();
    ComputeD12Vec(ws, d1Local, d2Local);

    AscendC::LocalTensor<int32_t> rejScratch =
        scratchU8[kD12WsInt32Active * sizeof(int32_t)].ReinterpretCast<int32_t>();
    BindAlg7RejVecWs(rejScratch, rejWs);

    aLocal = aHatQue.AllocTensor<int32_t>();
#if F203_ALG7_XOF_504
    // 504B 实验：向量 interleave ROM 按 168 pair 重生成后 Gather 仍待修；先用标量 rej 测 XOF tick。
    (void)rejWs;
    (void)ws.t0;
    (void)ws.t1;
    (void)RejScalarFromD12Ub(d1Local, d2Local, aLocal, F203Alg7::kCandPairs);
#else
    (void)RejVecBulkFromD12Ub(d1Local, d2Local, ws.t0, ws.t1, ws.c0, rejWs, aLocal);
#endif

    d1Que.FreeTensor(d1Local);
    d2Que.FreeTensor(d2Local);
    F203_AHAT16_PIPE_ALL();
}

/**
 * SEED_D → ρ → 本分片 poly（1 AIV×4 或 2 AIV×2/2）SHAKE + 向量 d12/rej → GM a_hat 对应块。
 *
 * 2 AIV：blockIdx 0 → polyIdx 0–1；blockIdx 1 → 2–3；GM 偏移互不重叠。
 */
/**
 * 单分片：blockIdx 0→poly 0–1，1→2–3（BLOCK_DIM=2）；BLOCK_DIM=1 时仅 blockIdx=0 有效。
 * 调用方须已 InitBuffer；ρ 由调用方提供（KeyGen 单 TPipe 路径可与 σ 同次 G 派生）。
 */
__aicore__ inline void BuildAHat16ShardWithUb(const uint8_t rho[F203Alg7::kRhoBytes], __gm__ int32_t *a_hat_gm,
                                                uint32_t blockIdx,
                                                AscendC::TBuf<AscendC::TPosition::VECCALC> &shakeXBuf,
                                                AscendC::TBuf<AscendC::TPosition::VECCALC> &shakeLenBuf,
                                                AscendC::TBuf<AscendC::TPosition::VECCALC> &shakeStagingBuf,
                                                AscendC::TBuf<AscendC::TPosition::VECCALC> &xofBuf,
                                                AscendC::TQue<AscendC::TPosition::VECOUT, 1> &d1Que,
                                                AscendC::TQue<AscendC::TPosition::VECOUT, 1> &d2Que,
                                                AscendC::TQue<AscendC::TPosition::VECOUT, 1> &aHatQue,
                                                AscendC::TBuf<AscendC::TPosition::VECCALC> &scratchBuf)
{
#if F203_AHAT16_BLOCK_DIM == 2
    const uint32_t polyBegin = (blockIdx == 0U) ? 0U : kShardPolysAiv0;
    const uint32_t polyCount = (blockIdx == 0U) ? kShardPolysAiv0 : kShardPolysAiv1;
    const uint32_t polyEnd = polyBegin + polyCount;
#else
    (void)blockIdx;
    const uint32_t polyBegin = 0U;
    const uint32_t polyEnd = kShakeBatch;
#endif

    AscendC::LocalTensor<uint8_t> xUb = shakeXBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lenUb = shakeLenBuf.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> stagingUb = shakeStagingBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint8_t> xofUb = xofBuf.Get<uint8_t>();

#if F203_AHAT16_BATCH_SHAKE
#error "F203_AHAT16_BATCH_SHAKE=1 not supported in BuildAHat16ShardWithUb (use BuildAHat16ShardFromSeedD)"
#endif

    AscendC::GlobalTensor<int32_t> aHatGm;
    aHatGm.SetGlobalBuffer(a_hat_gm, kAHatPolys * kKyberN);

    AscendC::LocalTensor<uint8_t> scratchU8 = scratchBuf.Get<uint8_t>();
    F203Alg7::Alg7RejVecWs rejWs{};
    AscendC::LocalTensor<int32_t> rejScratch =
        scratchU8[F203Alg7::kD12WsInt32Active * sizeof(int32_t)].ReinterpretCast<int32_t>();
    F203Alg7::BindAlg7RejVecWs(rejScratch, rejWs);
#if !F203_ALG7_XOF_504
    F203Alg7::InitAlg7InterleaveRomUb(rejWs.idxRom);
#endif
    F203_AHAT16_PIPE_ALL();

    for (uint32_t polyIdx = polyBegin; polyIdx < polyEnd; ++polyIdx) {
        uint8_t p = 0U;
        uint8_t j = 0U;
        PolyIdxToPJUb(polyIdx, p, j);

        F203Alg7::FillSampleSeedUb(rho, j, p, xUb, lenUb);
        F203Alg7::RunShake128SampleNttUb(xUb, lenUb, xofUb, stagingUb);
        F203_AHAT16_PIPE_ALL();

        AscendC::LocalTensor<int32_t> aLocal;
        SampleNttVecOnePolyFromXofUb(xofUb, scratchBuf, d1Que, d2Que, aHatQue, rejWs, aLocal);

        const uint32_t gmOff = AHatOffsetUb(static_cast<uint32_t>(p), static_cast<uint32_t>(j));
        aHatQue.EnQue(aLocal);
        aLocal = aHatQue.DeQue<int32_t>();
        F203_AHAT16_PIPE_ALL();
        AscendC::DataCopy(aHatGm[gmOff], aLocal, kKyberN);
        F203_AHAT16_PIPE_ALL();
        aHatQue.FreeTensor(aLocal);
    }
}

/**
 * 从 seed_d 派生 ρ 后构建 Â 分片（独立探针入口）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void BuildAHat16ShardFromSeedD(uint32_t seed_d, __gm__ int32_t *a_hat_gm, uint32_t blockIdx)
{
#if F203_AHAT16_BLOCK_DIM == 2
    const uint32_t polyBegin = (blockIdx == 0U) ? 0U : kShardPolysAiv0;
    const uint32_t polyCount = (blockIdx == 0U) ? kShardPolysAiv0 : kShardPolysAiv1;
    const uint32_t polyEnd = polyBegin + polyCount;
#else
    (void)blockIdx;
    const uint32_t polyBegin = 0U;
    const uint32_t polyEnd = kShakeBatch;
    const uint32_t polyCount = kShakeBatch;
#endif

    uint8_t rho[F203Alg7::kRhoBytes];
    F203Alg7::BuildRhoFromSeedD(seed_d, rho);

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeXBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeLenBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeStagingBuf;
#if F203_AHAT16_BATCH_SHAKE
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeYBuf;
#endif
    AscendC::TBuf<AscendC::TPosition::VECCALC> xofBuf;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> d1Que;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> d2Que;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> aHatQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;

#if F203_AHAT16_BATCH_SHAKE
    pipe.InitBuffer(shakeXBuf, kShakeBatchXUbBytes);
    pipe.InitBuffer(shakeLenBuf, kShakeBatchLenUbBytes);
#else
    constexpr uint32_t kShakeXUbBytes = 64U;
    constexpr uint32_t kShakeLenUbBytes = 32U;
    pipe.InitBuffer(shakeXBuf, kShakeXUbBytes);
    pipe.InitBuffer(shakeLenBuf, kShakeLenUbBytes);
#endif
    pipe.InitBuffer(shakeStagingBuf, F203Alg7::kShakeStagingUbBytes);
#if F203_AHAT16_BATCH_SHAKE
    pipe.InitBuffer(shakeYBuf, kShakeBatchYUbBytes);
#endif
    pipe.InitBuffer(xofBuf, F203Alg7::kXofUbBytes);
    pipe.InitBuffer(d1Que, 1, kD12Bytes);
    pipe.InitBuffer(d2Que, 1, kD12Bytes);
    pipe.InitBuffer(aHatQue, 1, kPolyAHatBytes);
    pipe.InitBuffer(scratchBuf, F203Alg7::kScratchInt32ElemsActive * sizeof(int32_t));

    AscendC::LocalTensor<uint8_t> xUb = shakeXBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lenUb = shakeLenBuf.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> stagingUb = shakeStagingBuf.Get<uint8_t>();
#if F203_AHAT16_BATCH_SHAKE
    AscendC::LocalTensor<uint8_t> yUb = shakeYBuf.Get<uint8_t>();
#endif
    AscendC::LocalTensor<uint8_t> xofUb = xofBuf.Get<uint8_t>();

#if F203_AHAT16_BATCH_SHAKE
    FillBatchSampleSeedsRangeUb(rho, xUb, lenUb, polyBegin, polyCount);
    ShakeGeneralTilingData tilingLocal{};
    ShakeXofUb::FillShakeTilingUb(tilingLocal, polyCount, kShakeMsgStride, F203Alg7::kXofBytes,
                                  SHAKE128_RATE_BYTES);
    tilingLocal.blockDim = 1U;
    ShakeXofUb::RunKernelShakeGeneralUb(xUb, lenUb, yUb, stagingUb, &tilingLocal);
    F203_AHAT16_PIPE_ALL();
#endif

#if !F203_AHAT16_BATCH_SHAKE
    BuildAHat16ShardWithUb(rho, a_hat_gm, blockIdx, shakeXBuf, shakeLenBuf, shakeStagingBuf, xofBuf, d1Que, d2Que,
                           aHatQue, scratchBuf);
#else
    AscendC::GlobalTensor<int32_t> aHatGm;
    aHatGm.SetGlobalBuffer(a_hat_gm, kAHatPolys * kKyberN);

    AscendC::LocalTensor<uint8_t> scratchU8 = scratchBuf.Get<uint8_t>();
    F203Alg7::Alg7RejVecWs rejWs{};
    AscendC::LocalTensor<int32_t> rejScratch =
        scratchU8[F203Alg7::kD12WsInt32Active * sizeof(int32_t)].ReinterpretCast<int32_t>();
    F203Alg7::BindAlg7RejVecWs(rejScratch, rejWs);
#if !F203_ALG7_XOF_504
    F203Alg7::InitAlg7InterleaveRomUb(rejWs.idxRom);
#endif
    F203_AHAT16_PIPE_ALL();

    for (uint32_t polyIdx = polyBegin; polyIdx < polyEnd; ++polyIdx) {
        uint8_t p = 0U;
        uint8_t j = 0U;
        PolyIdxToPJUb(polyIdx, p, j);

        const uint32_t localRow = polyIdx - polyBegin;
        CopyXofRowFromBatchUb(yUb, localRow, xofUb);

        AscendC::LocalTensor<int32_t> aLocal;
        SampleNttVecOnePolyFromXofUb(xofUb, scratchBuf, d1Que, d2Que, aHatQue, rejWs, aLocal);

        const uint32_t gmOff = AHatOffsetUb(static_cast<uint32_t>(p), static_cast<uint32_t>(j));
        aHatQue.EnQue(aLocal);
        aLocal = aHatQue.DeQue<int32_t>();
        F203_AHAT16_PIPE_ALL();
        AscendC::DataCopy(aHatGm[gmOff], aLocal, kKyberN);
        F203_AHAT16_PIPE_ALL();
        aHatQue.FreeTensor(aLocal);
    }
#endif
}

}  // namespace F203Ahat16
