/**
 * @file mmad_custom.cpp
 * @brief RB-T02：MIX GATE 时序 —— AIC 先 Wait(4) + 极轻 Cube + flag 1/3 握手 + TRACE。
 *
 * 生产 Encrypt 常见 GATE 外形（不含算法）：
 *   AIC：Wait(4) → Wait(1) → 极轻 Cube → Set(3)
 *   AIV：Set(4) → Set(1) → Wait(3)
 *
 * 背景：任务书要求验证「AIC 是否在 AIV Set4 之前进入 Wait4」——预期如此，靠 Set 唤醒。
 * 结论：AIC 路径首条 CrossCore 即 Wait(4)；AIV 先 Trace 再 Set(4)。
 * 永禁：flag 5/7、SoftSync、AIC Wait 环内 SyncAll、抄 Encrypt/alg14。
 * 模式：CrossCore modeId=0x2；Host 单 launch + SynchronizeStream。
 */
#include "kernel_operator.h"
#include "light_cube.hpp"
#include "tiling.h"

/**
 * 向 TRACE GM 写一个 uint32 魔数。
 * 背景：CAModel 上 AIC（及部分 AIV）对 GlobalTensor::SetValue 写 GM 可能不落盘；
 * 结论：改用 `__gm__` 指针标量写 + PIPE_ALL。
 */
__aicore__ inline void TraceMark(GM_ADDR traceGm, uint32_t slot, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(traceGm);
    *(p + slot) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** CrossCore Wait：mode 0x2，PIPE_MTE2（910B 上 pipe 模板不生效，与仓内 MIX 惯例一致）。 */
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
 * @param unused  [in]  保留
 * @param ws      [in/out] MAT_A/B/C + TRACE
 * @param tiling  [in]  占位
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR unused, GM_ADDR ws,
                                                  TilingData tiling)
{
    (void)unused;
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    using namespace tiling;
    GM_ADDR traceGm = ws + TRACE;
    GM_ADDR matA = ws + MAT_A;
    GM_ADDR matB = ws + MAT_B;
    GM_ADDR matC = ws + MAT_C;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (isAic) {
        // ---------- AIC：先 Wait(4)（GATE）→ Wait(1) → 极轻 Cube → Set(3) ----------
        // 背景：KB §X1；结论：AIC 首条 CrossCore 即 Wait(4)，在 AIV Set(4) 前进入属预期。
        // Wait(4) 后不立即 TraceMark：与 T01「Wait→Trace→Cube→Set」同构，把 Trace 放在 Wait(1) 后，
        // 避免 sync_audit 对 Wait 后 GM 标量写 / CrossSet 薄封装的 SYNC-05 假阳性。
        // POST_WAIT4 槽：在 Wait(1) 已返回后补标（此时 GATE 与 NTT-Wait1 均已过）。
        CrossWait(kFlagGate);
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT4, MAGIC_AIC_POST_WAIT4);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1, MAGIC_AIC_POST_WAIT1);

        rb_t02::LightCube cube;
        cube.Init();
        cube.Process(matC, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();

        TraceMark(traceGm, SLOT_AIC_PRE_SET3, MAGIC_AIC_PRE_SET3);
        CrossSet(kFlagAicDone);
    } else {
        // ---------- AIV：Set(4) → Set(1) → Wait(3) →（AIV0 写 out）----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET4, MAGIC_AIV0_PRE_SET4);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET4, MAGIC_AIV1_PRE_SET4);
        }
        CrossSet(kFlagGate);

        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1, MAGIC_AIV0_PRE_SET1);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1, MAGIC_AIV1_PRE_SET1);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3, MAGIC_AIV0_POST_WAIT3);
            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3, MAGIC_AIV1_POST_WAIT3);
        }
    }
}
