/**
 * @file sample_ntt_device.hpp
 * @brief RB-T17 设备侧 Â 薄壳：读 prep ρ → 活跃 lines3-7 积木 BuildAHat16ShardWithUb。
 *
 * 流水线位置：Launch2 MIX AIV0，CBD 之后、NTT(y) 之前。
 *
 * **硬锁** `F203_AHAT16_BLOCK_DIM=1`（#undef 后强制）：config 默认 2 时 AIV0 只写 8/16 poly
 * → a_hat max_abs≈3328 mism=2048/4096（T13 同症）。另须 npu_lib `ascendc_compile_definitions`。
 */
#ifndef RB_T17_SAMPLE_NTT_DEVICE_HPP
#define RB_T17_SAMPLE_NTT_DEVICE_HPP

// 本刀 MIX blockDim=1：单 AIV0 跑满 16 poly；禁止回落 config 默认 2
#ifdef F203_AHAT16_BLOCK_DIM
#undef F203_AHAT16_BLOCK_DIM
#endif
#define F203_AHAT16_BLOCK_DIM 1

#ifdef F203_AHAT16_BATCH_SHAKE
#undef F203_AHAT16_BATCH_SHAKE
#endif
#define F203_AHAT16_BATCH_SHAKE 0

#ifdef F203_ALG7_REJ_IMPL
#undef F203_ALG7_REJ_IMPL
#endif
#define F203_ALG7_REJ_IMPL 1

#ifdef F203_ALG7_D12_GATHER
#undef F203_ALG7_D12_GATHER
#endif
#define F203_ALG7_D12_GATHER 0

#ifdef F203_ALG7_XOF_504
#undef F203_ALG7_XOF_504
#endif
#define F203_ALG7_XOF_504 0

#include "f203_a_hat16_ub.hpp"
#include "kernel_operator.h"
#include "reenc/tiling.h"

#include <cstdint>

namespace rb_t15_ahat {

/**
 * AIV0：从 ws 读 Launch1 ρ[32]，跑 16× SampleNTT，写 Â 到 OFF_A_HAT。
 * @param ws 共享 workspace（OFF_RHO / OFF_A_HAT）
 * 前置：仅 AIV0；`F203_AHAT16_BLOCK_DIM==1`；禁 Host 预填最终 Â。
 */
__aicore__ inline void ComputeAHatSampleNtt(GM_ADDR ws)
{
    using namespace enc_tiling;

    uint8_t rho[F203Alg7::kRhoBytes];
    const __gm__ uint8_t *rhoGm = reinterpret_cast<const __gm__ uint8_t *>(ws + OFF_RHO);
    for (uint32_t i = 0U; i < F203Alg7::kRhoBytes; ++i) {
        rho[i] = rhoGm[i];
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeXBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeLenBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeStagingBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> xofBuf;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> d1Que;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> d2Que;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> aHatQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;

    constexpr uint32_t kShakeXUbBytes = 64U;
    constexpr uint32_t kShakeLenUbBytes = 32U;
    pipe.InitBuffer(shakeXBuf, kShakeXUbBytes);
    pipe.InitBuffer(shakeLenBuf, kShakeLenUbBytes);
    pipe.InitBuffer(shakeStagingBuf, F203Alg7::kShakeStagingUbBytes);
    pipe.InitBuffer(xofBuf, F203Alg7::kXofUbBytes);
    pipe.InitBuffer(d1Que, 1, F203Ahat16::kD12Bytes);
    pipe.InitBuffer(d2Que, 1, F203Ahat16::kD12Bytes);
    pipe.InitBuffer(aHatQue, 1, F203Ahat16::kPolyAHatBytes);
    pipe.InitBuffer(scratchBuf, F203Alg7::kScratchInt32ElemsActive * sizeof(int32_t));

    __gm__ int32_t *aHatGm = reinterpret_cast<__gm__ int32_t *>(ws + OFF_A_HAT);
    // blockIdx=0 + BLOCK_DIM=1 → poly 0..15 全写
    F203Ahat16::BuildAHat16ShardWithUb(rho, aHatGm, 0U, shakeXBuf, shakeLenBuf, shakeStagingBuf, xofBuf, d1Que,
                                       d2Que, aHatQue, scratchBuf);
    AscendC::PipeBarrier<PIPE_ALL>();
}

} // namespace rb_t15_ahat

#endif
