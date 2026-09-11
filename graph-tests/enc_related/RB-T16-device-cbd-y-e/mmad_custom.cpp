/**
 * @file mmad_custom.cpp
 * @brief RB-T16：MIX 设备 coins→(y,e₁,e₂) — 握手(1/3) → AIV0 PRF+Alg.8 CBD η=2。
 *
 * 生产 Encrypt 行 8–15 外形（设备采噪声；禁 Host 预喂最终 y/e）：
 *   AIC：Wait(1)→CubeSample→Set(3)
 *   AIV：Set(1)→Wait(3) → PRF(coins)→CBD → y‖e1‖e2（仅 AIV0 写结果）
 *
 * 背景：T07 Host coins 契约；T13 MIX 半链；本刀关设备 CBD。
 * 结论：flag 仅 1/3；永禁 5/7；主验收 y/e1/e2 对拍。
 * 模式：CrossCore modeId=0x2；Host 单 launch + SynchronizeStream。
 *
 * 若挂死 TRACE 假设：
 *   - 无 POST_WAIT3 → 卡 CBD 握手
 *   - 有 POST_WAIT3 无 CBD_DONE → 卡 PRF/CBD / 写出
 */
#include "cbd_device.hpp"
#include "kernel_operator.h"
#include "light_cube.hpp"
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
 * @param out    [out] 握手成功魔数（AIV0 写 4B）
 * @param yeeOut [out] 设备算出的 y‖e1‖e2（int32，9216B）；Host 禁预填最终值
 * @param ws     [in/out] 预喂 coins + mat + TRACE；设备写 PRF 中间区
 * @param tiling [in] 占位
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR yeeOut, GM_ADDR ws,
                                                  TilingData tiling)
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
        rb_t16::LightCube cube;
        cube.Init();

        // ---------- CBD 握手：Wait(1) → Cube → Set(3) ----------
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
            // ---------- AIV0：coins→PRF→CBD→y/e1/e2 ----------
            rb_t16::ComputeYeeCbd(ws, yeeOut);
            TraceMark(traceGm, SLOT_AIV0_CBD_DONE, MAGIC_AIV0_CBD_DONE);

            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3, MAGIC_AIV1_POST_WAIT3);
        }
    }
}
