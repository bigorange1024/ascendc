/**
 * @file mmad_custom.cpp
 * @brief RB-T01：MIX（1AIC+2AIV）最短 CrossCore 握手 + 极轻 Cube + TRACE。
 *
 * 同构 Encrypt NTT 的 flag 1/3 节奏（不含算法）：
 *   双 AIV  SET(1) → AIC WAIT(1) → 极轻 Cube → AIC SET(3) → 双 AIV WAIT(3)
 *
 * 永禁：flag 5/7、SoftSync、AIC Wait 环内 SyncAll、抄 Encrypt/alg14 实现。
 * 模式：CrossCore modeId=0x2（同核 AIC↔全体 AIV）；Host 单 launch + SynchronizeStream。
 */
#include "kernel_operator.h"
#include "light_cube.hpp"
#include "tiling.h"

/**
 * 向 TRACE GM 写一个 uint32 魔数。
 * 背景：CAModel 上 AIC（及部分 AIV）对 GlobalTensor::SetValue 写 GM 可能不落盘，
 * 导致 D2H 后 TRACE 槽为 0；结论：改用 `__gm__` 指针标量写 + PIPE_ALL。
 * @param traceGm TRACE 区基址（ws+TRACE）
 * @param slot    槽位下标（见 tiling / trace_map.md）
 * @param magic   魔数
 */
__aicore__ inline void TraceMark(GM_ADDR traceGm, uint32_t slot, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(traceGm);
    *(p + slot) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * CrossCore Wait：mode 0x2，PIPE_MTE2（与仓内 MIX 探针惯例一致；910B 上 pipe 模板不生效）。
 * @param flagId 与对端 Set 配对的 flag（1 或 3）
 */
__aicore__ inline void CrossWait(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * CrossCore Set：mode 0x2，PIPE_MTE2。
 * @param flagId 与对端 Wait 配对的 flag（1 或 3）
 */
__aicore__ inline void CrossSet(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2；blockDim=1。
 * @param out     [out] 握手成功魔数（AIV0 写 4B）
 * @param unused  [in]  保留（未用，满足壳参数位）
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

    // MIX：GetSubBlockNum()==1 → AIC（Cube）；否则 AIV，GetSubBlockIdx 区分 0/1。
    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (isAic) {
        // ---------- AIC：WAIT(1) → 极轻 Cube → SET(3) ----------
        // 背景：任务书最短握手；结论：AIC 禁止在 Wait 等待环内 SyncAll（KB §B1）。
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1, MAGIC_AIC_POST_WAIT1);

        rb_t01::LightCube cube;
        cube.Init();
        cube.Process(matC, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();

        TraceMark(traceGm, SLOT_AIC_PRE_SET3, MAGIC_AIC_PRE_SET3);
        CrossSet(kFlagAicDone);
    } else {
        // ---------- AIV：TRACE → SET(1) → WAIT(3) →（AIV0 写 out）----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1, MAGIC_AIV0_PRE_SET1);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1, MAGIC_AIV1_PRE_SET1);
        }
        // 双 AIV 均 SET(1)；mode 0x2 下 AIC 单次 WAIT 与双 AIV SET 为仓内 MIX 惯例配对。
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3, MAGIC_AIV0_POST_WAIT3);
            // AIV0 写 out 冒烟魔数（4 字节 LE）；同样用 __gm__ 标量写保证 SIM 可见。
            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3, MAGIC_AIV1_POST_WAIT3);
        }
    }
}
