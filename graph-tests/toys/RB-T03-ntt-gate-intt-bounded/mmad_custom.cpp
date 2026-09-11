/**
 * @file mmad_custom.cpp
 * @brief RB-T03：MIX NTT(1/3) → GATE Wait(4) → INTT(复用 1/3) + 两段极轻 Cube + TRACE。
 *
 * 生产 Encrypt 常见外形（不含算法）：
 *   AIC：Wait(1)→CubeNTT→Set(3) → Wait(4) → Wait(1)→CubeINTT→Set(3)
 *   AIV：Set(1)→Wait(3) → Set(4) → Set(1)→Wait(3)
 *
 * 背景：T02 已验证 GATE 先 Wait(4)；本刀在 GATE 两侧各挂一段 1/3 握手与有界 Cube。
 * 结论：INTT 复用 flag 1/3（NTT 的 Wait(3) 完成后才复用）；永禁 5/7。
 * 模式：CrossCore modeId=0x2；Host 单 launch + SynchronizeStream。
 *
 * 若挂死 TRACE 假设（FEEDBACK 亦录）：
 *   - 卡在 NTT：AIV0_PRE_SET1_NTT 有、POST_WAIT3_NTT 无 → Wait(1)/Set(3) 断
 *   - 卡在 GATE：POST_WAIT3_NTT 有、PRE_SET4 有、但 INTT 槽全无 → Wait(4) 未醒
 *   - 卡在 INTT：PRE_SET1_INTT 有、POST_WAIT3_INTT 无 → 复用 1/3 断
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
 * @param ws      [in/out] MAT_A/B + MAT_C_NTT/INTT + TRACE
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
    GM_ADDR matCNtt = ws + MAT_C_NTT;
    GM_ADDR matCIntt = ws + MAT_C_INTT;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (isAic) {
        // 单 LightCube / 单 TPipe：两段 Process 写不同 mat_c，避免双 TPipe 冲突。
        rb_t03::LightCube cube;
        cube.Init();

        // ---------- 段1 NTT：Wait(1) → Cube → Set(3) ----------
        // Trace 放在 Wait 返回后，避免 sync_audit 对 Wait 后立刻 GM 写的假阳性。
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1_NTT, MAGIC_AIC_POST_WAIT1_NTT);

        cube.Process(matCNtt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();

        TraceMark(traceGm, SLOT_AIC_PRE_SET3_NTT, MAGIC_AIC_PRE_SET3_NTT);
        CrossSet(kFlagAicDone);

        // ---------- 段2 GATE：Wait(4)（由 AIV Set4 唤醒；先于 Set4 进入属预期）----------
        CrossWait(kFlagGate);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT4, MAGIC_AIC_POST_WAIT4);

        // ---------- 段3 INTT：复用 Wait(1) → Cube → Set(3) ----------
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1_INTT, MAGIC_AIC_POST_WAIT1_INTT);

        cube.Process(matCIntt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();

        TraceMark(traceGm, SLOT_AIC_PRE_SET3_INTT, MAGIC_AIC_PRE_SET3_INTT);
        CrossSet(kFlagAicDone);
    } else {
        // ---------- AIV 段1 NTT：Set(1) → Wait(3) ----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1_NTT, MAGIC_AIV0_PRE_SET1_NTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1_NTT, MAGIC_AIV1_PRE_SET1_NTT);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        // NTT WAIT3 后成对 Trace：AIV0→槽5、AIV1→槽16；验收接受任一侧（NPU 勿硬绑 AIV0）。
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3_NTT, MAGIC_AIV0_POST_WAIT3_NTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3_NTT, MAGIC_AIV1_POST_WAIT3_NTT);
        }

        // ---------- AIV 段2 GATE：Set(4) ----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET4, MAGIC_AIV0_PRE_SET4);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET4, MAGIC_AIV1_PRE_SET4);
        }
        CrossSet(kFlagGate);

        // ---------- AIV 段3 INTT：复用 Set(1) → Wait(3) ----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1_INTT, MAGIC_AIV0_PRE_SET1_INTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1_INTT, MAGIC_AIV1_PRE_SET1_INTT);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3_INTT, MAGIC_AIV0_POST_WAIT3_INTT);
            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3_INTT, MAGIC_AIV1_POST_WAIT3_INTT);
        }
    }
}
