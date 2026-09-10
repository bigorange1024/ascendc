/**
 * @file d09_decrypt_fused_custom.cpp
 * @brief RB-D09：单 MIX 融合核 — PrepUnpack → SyncAll → NTT+su_dot → INTT+extract → m。
 *
 * 本文件在流水线中的位置：唯一设备核；Host 仅一次 ACLRT_LAUNCH。
 *
 * 设备流：
 *   AIV0: PrepUnpack(dk,c→ŝ/u/v) + TRACE PREP
 *   ALL:  SyncAll()  // CrossCore Wait 环外
 *   AIC:  Wait(1)→Cube NTT→Set(3)；Wait(1)→Cube INTT→Set(3)
 *   AIV:  Set(1)；Wait(3)；AIV0 ComputeNttAndSuDot；
 *         Set(1)；Wait(3)；AIV0 ComputeInttAndExtract→m；写 MAGIC
 *   AIV1: 仅握手（不等 AIV0 数学）
 *
 * 硬锁：flag∈{1,3}；禁 SoftSync；禁 Wait 环内 SyncAll；BLOCK_DIM=1。
 * 未采用：拆成 2/3 launch；flag 5/7。
 */
#include "kernel_operator.h"
#include "light_cube.hpp"
#include "ntt_device_math.hpp"
#include "intt_device_math.hpp"
#include "prep_device_math.hpp"
#include "tiling.h"

/** TRACE 槽写入（标量 GM；仅诊断，非业务输出）。 */
__aicore__ inline void TraceMark(GM_ADDR traceGm, uint32_t slot, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(traceGm);
    *(p + slot) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void CrossWait(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void CrossSet(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * 融合 MIX：prep → SyncAll → NTT+dot → INTT+extract。
 * @param out   [out] MAGIC_OUT_OK=0x44303931（"D091"，AIV0 写）
 * @param mOut  [out] m[32]
 * @param dkIn  [in]  dk_pke[1536]（H2D）
 * @param cIn   [in]  c[1568]（H2D）
 * @param ws    [in/out] 统一 d09 workspace（Host 预喂 ζ/γ/mat）
 * @param tiling 占位
 */
extern "C" __global__ __aicore__ void d09_decrypt_fused_custom(GM_ADDR out, GM_ADDR mOut,
                                                               GM_ADDR dkIn, GM_ADDR cIn,
                                                               GM_ADDR ws, D09TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    using namespace d09;
    GM_ADDR traceGm = ws + OFF_TRACE;
    GM_ADDR matA = ws + OFF_MAT_A;
    GM_ADDR matB = ws + OFF_MAT_B;
    GM_ADDR matCNtt = ws + OFF_MAT_C_NTT;
    GM_ADDR matCIntt = ws + OFF_MAT_C_INTT;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    // Segment0: AIV0 prep；AIC/AIV1 空转；SyncAll 在 CrossCore Wait 环外
    if (!isAic && subIdx == 0) {
        rb_d09::PrepUnpackDecrypt(dkIn, cIn, ws);
        AscendC::PipeBarrier<PIPE_ALL>();
        TraceMark(traceGm, SLOT_PREP_DONE, MAGIC_PREP_DONE);
        AscendC::PipeBarrier<PIPE_ALL>();
        __gm__ uint32_t *pm = reinterpret_cast<__gm__ uint32_t *>(ws + OFF_PREP_MARK);
        *pm = MAGIC_PREP_MARK;
        AscendC::PipeBarrier<PIPE_ALL>();
    }
    AscendC::SyncAll();

    if (isAic) {
        rb_d09::LightCube cube;
        cube.Init();

        // ---- 段1 NTT 握手（flag 1/3）----
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1_NTT, MAGIC_AIC_POST_WAIT1_NTT);
        cube.Process(matCNtt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();
        TraceMark(traceGm, SLOT_AIC_PRE_SET3_NTT, MAGIC_AIC_PRE_SET3_NTT);
        CrossSet(kFlagAicDone);

        // ---- 段2 INTT 握手（复用同组 flag；完整一轮后可复用）----
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1_INTT, MAGIC_AIC_POST_WAIT1_INTT);
        cube.Process(matCIntt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();
        TraceMark(traceGm, SLOT_AIC_PRE_SET3_INTT, MAGIC_AIC_PRE_SET3_INTT);
        CrossSet(kFlagAicDone);
    } else {
        // ---- 段1 AIV ----
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1_NTT, MAGIC_AIV0_PRE_SET1_NTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1_NTT, MAGIC_AIV1_PRE_SET1_NTT);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3_NTT, MAGIC_AIV0_POST_WAIT3_NTT);
            rb_d09::ComputeNttAndSuDot(ws);
            TraceMark(traceGm, SLOT_AIV0_NTT_DOT_DONE, MAGIC_AIV0_NTT_DOT_DONE);
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3_NTT, MAGIC_AIV1_POST_WAIT3_NTT);
        }

        // ---- 段2 AIV（AIV1 不等 AIV0 数学）----
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1_INTT, MAGIC_AIV0_PRE_SET1_INTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1_INTT, MAGIC_AIV1_PRE_SET1_INTT);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3_INTT, MAGIC_AIV0_POST_WAIT3_INTT);
            rb_d09::ComputeInttAndExtract(ws, mOut);
            TraceMark(traceGm, SLOT_AIV0_EXTRACT_DONE, MAGIC_AIV0_EXTRACT_DONE);

            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3_INTT, MAGIC_AIV1_POST_WAIT3_INTT);
        }
    }
}
