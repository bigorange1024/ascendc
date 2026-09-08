/**
 * @file sample_ntt_device.hpp
 * @brief RB-T13 设备侧 Â 生成薄壳：读 Host ρ → 调用活跃 Alg.13 行 3–7 积木。
 *
 * 流水线位置：MIX AIV0 在 flag 1/3 握手之后调用；写出 a_hat[16,256] int32。
 *
 * 积木接线（**不**大段抄探针实现）：
 *   - include 路径指向 `pass-fix-f203-alg13-lines3-7-a-hat-k4` / `pass-fix-f203-alg7-sample-ntt-k4`
 *   - 调用 `F203Ahat16::BuildAHat16ShardWithUb(ρ, …, blockIdx=0)`（本刀 `F203_AHAT16_BLOCK_DIM=1`）
 *
 * 背景：T07 已定义 ρ←ek 尾；本刀 Host 只预喂 ρ，禁预喂最终 Â；禁设备再跑 Phase G。
 * 结论：I/O 契约对齐 lines3-7；MIX 壳对齐 T12。
 */
#ifndef RB_T13_SAMPLE_NTT_DEVICE_HPP
#define RB_T13_SAMPLE_NTT_DEVICE_HPP

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

namespace rb_t13 {

/**
 * AIV0：从 ws 读 ρ[32]，跑 16× SampleNTT，写 Â 到 aHatOut 与 ws 镜像区。
 *
 * @param ws      共享 workspace（含 OFF_RHO / OFF_A_HAT）
 * @param aHatOut 独立输出 GM（Host 对拍用；启动前须清零）
 *
 * 前置：仅 AIV0 调用；`F203_AHAT16_BLOCK_DIM==1`（单核跑满 16 poly）。
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

    // ---------- 3) 积木：ρ → 16 poly â → 独立 aHatOut（Host verify 直读）----------
    // 背景：禁 GM→GM DataCopy；与 T12 不同，本刀不镜像到 ws（ws 仅留 ρ/mat/TRACE）。
    __gm__ int32_t *aHatGm = reinterpret_cast<__gm__ int32_t *>(aHatOut);
    F203Ahat16::BuildAHat16ShardWithUb(rho, aHatGm, 0U, shakeXBuf, shakeLenBuf, shakeStagingBuf, xofBuf, d1Que,
                                       d2Que, aHatQue, scratchBuf);
    AscendC::PipeBarrier<PIPE_ALL>();
}

} // namespace rb_t13

#endif
