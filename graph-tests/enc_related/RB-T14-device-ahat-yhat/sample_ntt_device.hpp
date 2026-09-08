/**
 * @file sample_ntt_device.hpp
 * @brief RB-T14 设备侧 Â 薄壳：读 Host ρ → 调用活跃 lines3-7 积木。
 *
 * 流水线位置：MIX AIV0 在 flag 1/3 握手之后、NTT(y) 之前调用。
 * 积木接线（**不**大段抄探针实现）：CMake `-I` + `BuildAHat16ShardWithUb`。
 *
 * 背景：T13 已证单 AIV0 + `F203_AHAT16_BLOCK_DIM=1`；本刀复用该契约。
 * 结论：禁 Host 预喂最终 Â；禁抄 alg14/encrypt/frozen。
 */
#ifndef RB_T14_SAMPLE_NTT_DEVICE_HPP
#define RB_T14_SAMPLE_NTT_DEVICE_HPP

// 本刀 MIX blockDim=1：必须在 include 积木头之前锁定单 AIV 跑满 16 poly。
// （ascendc_library 若不吃 target_compile_definitions，会回落到 config 默认 2 → 只写半边 Â）
#ifndef F203_AHAT16_BLOCK_DIM
#define F203_AHAT16_BLOCK_DIM 1
#endif
#ifndef F203_AHAT16_BATCH_SHAKE
#define F203_AHAT16_BATCH_SHAKE 0
#endif
#ifndef F203_ALG7_REJ_IMPL
#define F203_ALG7_REJ_IMPL 1
#endif
#ifndef F203_ALG7_D12_GATHER
#define F203_ALG7_D12_GATHER 0
#endif
#ifndef F203_ALG7_XOF_504
#define F203_ALG7_XOF_504 0
#endif

#include "f203_a_hat16_ub.hpp"
#include "kernel_operator.h"
#include "tiling.h"

#include <cstdint>

namespace rb_t14 {

/**
 * AIV0：从 ws 读 ρ[32]，跑 16× SampleNTT，写 Â 到 aHatOut。
 *
 * @param ws      共享 workspace（含 OFF_RHO）
 * @param aHatOut 独立输出 GM（Host 对拍用；启动前须清零）
 *
 * 前置：仅 AIV0 调用；`F203_AHAT16_BLOCK_DIM==1`。
 * 说明：本函数内自建 TPipe；返回后 UB 释放，供后续 NTT(y) 再建管。
 */
__aicore__ inline void ComputeAHatSampleNtt(GM_ADDR ws, GM_ADDR aHatOut)
{
    using namespace tiling;

    // ---------- 1) 从 GM 读 Host 预喂 ρ（标量；32B）----------
    uint8_t rho[F203Alg7::kRhoBytes];
    const __gm__ uint8_t *rhoGm = reinterpret_cast<const __gm__ uint8_t *>(ws + OFF_RHO);
    for (uint32_t i = 0U; i < F203Alg7::kRhoBytes; ++i) {
        rho[i] = rhoGm[i];
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---------- 2) 单 TPipe：与 lines3-7 逐条 SHAKE 路径同尺寸闭包 ----------
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

    // ---------- 3) 积木：ρ → 16 poly â → 独立 aHatOut ----------
    __gm__ int32_t *aHatGm = reinterpret_cast<__gm__ int32_t *>(aHatOut);
    F203Ahat16::BuildAHat16ShardWithUb(rho, aHatGm, 0U, shakeXBuf, shakeLenBuf, shakeStagingBuf, xofBuf, d1Que,
                                       d2Que, aHatQue, scratchBuf);
    AscendC::PipeBarrier<PIPE_ALL>();
}

} // namespace rb_t14

#endif
