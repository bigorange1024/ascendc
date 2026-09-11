/**
 * @file mmad_custom.cpp
 * @brief RB-T14：MIX 设备 Â+ŷ — 握手(1/3) → AIV0 SampleNTT 再 NTT(y)。
 *
 * 生产 Encrypt 行 3–7 ‖ 行 16 外形（设备算 Â 与 ŷ；禁 Host 预喂最终值）：
 *   AIC：Wait(1)→Cube→Set(3)
 *   AIV：Set(1)→Wait(3) → Â←SampleNTT(ρ) → ŷ←NTT(y)（仅 AIV0）
 *
 * 背景：T12/T13 各自绿；本刀单 launch 同核顺序拼装。
 * 结论：flag 仅 1/3；永禁 5/7；主验收 Â 与 ŷ 双对拍。
 * 模式：CrossCore modeId=0x2；Host 单 launch + SynchronizeStream。
 *
 * 若挂死 TRACE 假设：
 *   - 无 POST_WAIT3 → 卡握手
 *   - 有 POST_WAIT3 无 AHAT_DONE → 卡 SampleNTT
 *   - 有 AHAT 无 YHAT_DONE → 卡 ForwardNTT / 写出
 */
#include "kernel_operator.h"
#include "light_cube.hpp"
#include "ntt_device_math.hpp"
#include "sample_ntt_device.hpp"
#include "tiling.h"

/** TRACE 标量写（CAModel 上 AIC/部分 AIV 对 GlobalTensor::SetValue 可能不落盘）。 */
__aicore__ inline void TraceMark(GM_ADDR traceGm, uint32_t slot, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(traceGm);
    *(p + slot) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** CrossCore Wait：mode 0x2，PIPE_MTE2（910B 上 pipe 模板不生效）。 */
__aicore__ inline void CrossWait(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** CrossCore Set：mode 0x2，PIPE_MTE2。 */
__aicore__ inline void CrossSet(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2；blockDim=1。
 * @param out     [out] 握手成功魔数（AIV0 写 4B）
 * @param aHatOut [out] 设备 Â（int32，16384B）；Host 禁预填最终值
 * @param yHatOut [out] 设备 ŷ（int32，4096B）；Host 禁预填最终值
 * @param ws      [in/out] 预喂 ρ/y/ζ + mat + TRACE；设备写 Y_HAT 镜像
 * @param tiling  [in] 占位
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR aHatOut, GM_ADDR yHatOut,
                                                  GM_ADDR ws, TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    using namespace tiling;
    GM_ADDR traceGm = ws + OFF_TRACE;
    GM_ADDR matA = ws + OFF_MAT_A;
    GM_ADDR matB = ws + OFF_MAT_B;
    GM_ADDR matC = ws + OFF_MAT_C;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (isAic) {
        rb_t14::LightCube cube;
        cube.Init();

        // ---------- 握手：Wait(1) → Cube → Set(3) ----------
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1, MAGIC_AIC_POST_WAIT1);

        cube.Process(matC, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();

        TraceMark(traceGm, SLOT_AIC_PRE_SET3, MAGIC_AIC_PRE_SET3);
        CrossSet(kFlagAicDone);
    } else {
        // ---------- AIV 握手：Set(1) → Wait(3) ----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1, MAGIC_AIV0_PRE_SET1);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1, MAGIC_AIV1_PRE_SET1);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3, MAGIC_AIV0_POST_WAIT3);

            // ---------- AIV0 同核顺序：先 Â 再 ŷ（两路独立 TPipe）----------
            rb_t14::ComputeAHatSampleNtt(ws, aHatOut);
            TraceMark(traceGm, SLOT_AIV0_AHAT_DONE, MAGIC_AIV0_AHAT_DONE);

            rb_t14::ComputeYHatNtt(ws, yHatOut);
            TraceMark(traceGm, SLOT_AIV0_YHAT_DONE, MAGIC_AIV0_YHAT_DONE);

            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3, MAGIC_AIV1_POST_WAIT3);
        }
    }
}
