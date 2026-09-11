/**
 * @file t30_dec_fused_custom.cpp
 * @brief RB-T30 L1：单 MIX Decrypt 全链 — Unpack → SyncAll → NTT+dot → INTT+extract → m'。
 *
 * 设备流：
 *   AIV0: UnpackDecryptInputs(dk,c→ŝ/u/v) + TRACE PREP
 *   ALL:  SyncAll()  // CrossCore Wait 环外
 *   AIC:  Wait(1)→Cube NTT→Set(3)；Wait(1)→Cube INTT→Set(3)
 *   AIV:  Set(1)；Wait(3)；AIV0 RunNttSuDot；
 *         Set(1)；Wait(3)；AIV0 RunInttExtractM→m'；写 MAGIC
 *   AIV1: 仅握手
 *
 * 硬锁：flag∈{1,3}；禁 SoftSync；禁 Wait 环内 SyncAll；BLOCK_DIM=1。
 * basename：t30_dec_fused_custom（全局唯一）。
 */
#include "decrypt/decrypt_math.hpp"
#include "kernel_operator.h"
#include "light_cube.hpp"
#include "tiling.h"

/** TRACE 槽写入（诊断；非业务输出）。 */
__aicore__ inline void DecTrace(GM_ADDR traceGm, uint32_t slot, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(traceGm);
    *(p + slot) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void DecCrossWait(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void DecCrossSet(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * L1 融合 MIX：prep → SyncAll → NTT+dot → INTT+extract。
 * @param out   [out] MAGIC_OUT_OK=0x54333032（AIV0）
 * @param mOut  [out] m'[32]
 * @param dkIn  [in]  dk_pke[1536]
 * @param cIn   [in]  c[1568]
 * @param ws    [in/out] t30_dec workspace（Host 预喂 ζ/γ/mat）
 * @param tiling 占位
 */
extern "C" __global__ __aicore__ void t30_dec_fused_custom(GM_ADDR out, GM_ADDR mOut, GM_ADDR dkIn,
                                                           GM_ADDR cIn, GM_ADDR ws,
                                                           T30DecTilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    using namespace t30_dec;
    GM_ADDR traceGm = ws + OFF_TRACE;
    GM_ADDR matA = ws + OFF_MAT_A;
    GM_ADDR matB = ws + OFF_MAT_B;
    GM_ADDR matCNtt = ws + OFF_MAT_C_NTT;
    GM_ADDR matCIntt = ws + OFF_MAT_C_INTT;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    // ---- 段0：AIV0 unpack；SyncAll 在 Wait 环外 ----
    if (!isAic && subIdx == 0) {
        rb_t30::UnpackDecryptInputs(dkIn, cIn, ws);
        AscendC::PipeBarrier<PIPE_ALL>();
        DecTrace(traceGm, SLOT_PREP_DONE, MAGIC_PREP_DONE);
        AscendC::PipeBarrier<PIPE_ALL>();
        __gm__ uint32_t *pm = reinterpret_cast<__gm__ uint32_t *>(ws + OFF_PREP_MARK);
        *pm = MAGIC_PREP_MARK;
        AscendC::PipeBarrier<PIPE_ALL>();
    }
    AscendC::SyncAll();

    if (isAic) {
        rb_t30::T30LightCube cube;
        cube.Init();

        DecCrossWait(kFlagAivReady);
        DecTrace(traceGm, SLOT_AIC_POST_WAIT1_NTT, MAGIC_AIC_POST_WAIT1_NTT);
        cube.RunOnce(matCNtt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();
        DecTrace(traceGm, SLOT_AIC_PRE_SET3_NTT, MAGIC_AIC_PRE_SET3_NTT);
        DecCrossSet(kFlagAicDone);

        DecCrossWait(kFlagAivReady);
        DecTrace(traceGm, SLOT_AIC_POST_WAIT1_INTT, MAGIC_AIC_POST_WAIT1_INTT);
        cube.RunOnce(matCIntt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();
        DecTrace(traceGm, SLOT_AIC_PRE_SET3_INTT, MAGIC_AIC_PRE_SET3_INTT);
        DecCrossSet(kFlagAicDone);
    } else {
        if (subIdx == 0) {
            DecTrace(traceGm, SLOT_AIV0_PRE_SET1_NTT, MAGIC_AIV0_PRE_SET1_NTT);
        } else {
            DecTrace(traceGm, SLOT_AIV1_PRE_SET1_NTT, MAGIC_AIV1_PRE_SET1_NTT);
        }
        DecCrossSet(kFlagAivReady);

        DecCrossWait(kFlagAicDone);
        if (subIdx == 0) {
            DecTrace(traceGm, SLOT_AIV0_POST_WAIT3_NTT, MAGIC_AIV0_POST_WAIT3_NTT);
            rb_t30::RunNttSuDot(ws);
            DecTrace(traceGm, SLOT_AIV0_NTT_DOT_DONE, MAGIC_AIV0_NTT_DOT_DONE);
        } else {
            DecTrace(traceGm, SLOT_AIV1_POST_WAIT3_NTT, MAGIC_AIV1_POST_WAIT3_NTT);
        }

        if (subIdx == 0) {
            DecTrace(traceGm, SLOT_AIV0_PRE_SET1_INTT, MAGIC_AIV0_PRE_SET1_INTT);
        } else {
            DecTrace(traceGm, SLOT_AIV1_PRE_SET1_INTT, MAGIC_AIV1_PRE_SET1_INTT);
        }
        DecCrossSet(kFlagAivReady);

        DecCrossWait(kFlagAicDone);
        if (subIdx == 0) {
            DecTrace(traceGm, SLOT_AIV0_POST_WAIT3_INTT, MAGIC_AIV0_POST_WAIT3_INTT);
            rb_t30::RunInttExtractM(ws, mOut);
            DecTrace(traceGm, SLOT_AIV0_EXTRACT_DONE, MAGIC_AIV0_EXTRACT_DONE);

            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            DecTrace(traceGm, SLOT_AIV1_POST_WAIT3_INTT, MAGIC_AIV1_POST_WAIT3_INTT);
        }
    }
}
