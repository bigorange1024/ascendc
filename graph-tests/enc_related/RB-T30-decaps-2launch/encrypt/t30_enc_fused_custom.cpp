/**
 * @file t30_enc_fused_custom.cpp
 * @brief RB-T30 L2：单 MIX Re-Encrypt/Encaps 全链（原 enc_prep+enc_compute 融合）。
 *
 * 设备流：
 *   AIV0: PrepEncapsPrefix(ek/G) + PrepEncapsHeavy(μ/Decode/CBD/Â) + TRACE
 *   ALL:  SyncAll()  // Wait 环外
 *   AIC:  Wait(1)→Cube NTT→Set(3)；Wait(4)；Wait(1)→Cube INTT→Set(3)
 *   AIV:  Set(1)；Wait(3)；AIV0 ŷ+Mul；Set(4)；Set(1)；Wait(3)；INTT+噪+pack
 *   AIV1: 仅握手
 *
 * Flag：1/3 + 4=GATE；永禁 5/7；BLOCK_DIM=1。
 * basename：t30_enc_fused_custom。
 */
#ifdef F203_AHAT16_BLOCK_DIM
#undef F203_AHAT16_BLOCK_DIM
#endif
#define F203_AHAT16_BLOCK_DIM 1

#include "encrypt/encrypt_math.hpp"
#include "kernel_operator.h"
#include "light_cube.hpp"
#include "tiling.h"

__aicore__ inline void EncTrace(GM_ADDR traceGm, uint32_t slot, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(traceGm);
    *(p + slot) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void EncCrossWait(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void EncCrossSet(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * L2 融合 MIX：prep 前缀 → SyncAll → NTT 路径两轮 flag1/3（含 GATE4）。
 * @param out   [out] MAGIC_OUT_OK
 * @param cOut  [out] c'[1568]
 * @param ekIn  [in]  ek_PKE[1568]
 * @param ws    [in/out] t30_enc workspace（Host 预装 m'/ζ/γ/mat）
 * @param tiling 占位
 */
extern "C" __global__ __aicore__ void t30_enc_fused_custom(GM_ADDR out, GM_ADDR cOut, GM_ADDR ekIn,
                                                           GM_ADDR ws, T30EncTilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    using namespace t30_enc;
    GM_ADDR traceGm = ws + OFF_TRACE;
    GM_ADDR matA = ws + OFF_MAT_A;
    GM_ADDR matB = ws + OFF_MAT_B;
    GM_ADDR matCNtt = ws + OFF_MAT_C_NTT;
    GM_ADDR matCIntt = ws + OFF_MAT_C_INTT;
    GM_ADDR uGm = ws + OFF_U;
    GM_ADDR vGm = ws + OFF_V;
    GM_ADDR cWs = ws + OFF_C;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    // ---- 段0：AIV0 prep（G/CBD/Â…）；SyncAll 在 Wait 环外 ----
    if (!isAic && subIdx == 0) {
        rb_t30::PrepEncapsPrefix(ekIn, ws);
        EncTrace(traceGm, SLOT_AIV0_G_DONE, MAGIC_AIV0_G_DONE);
        rb_t30::PrepEncapsHeavy(ws);
        EncTrace(traceGm, SLOT_AIV0_MU_DONE, MAGIC_AIV0_MU_DONE);
        EncTrace(traceGm, SLOT_AIV0_BD12_DONE, MAGIC_AIV0_BD12_DONE);
        EncTrace(traceGm, SLOT_AIV0_CBD_DONE, MAGIC_AIV0_CBD_DONE);
        EncTrace(traceGm, SLOT_AIV0_AHAT_DONE, MAGIC_AIV0_AHAT_DONE);
        EncTrace(traceGm, SLOT_PREP_DONE, MAGIC_PREP_DONE);
        __gm__ uint32_t *pm = reinterpret_cast<__gm__ uint32_t *>(ws + OFF_PREP_MARK);
        *pm = MAGIC_PREP_MARK;
        AscendC::PipeBarrier<PIPE_ALL>();
    }
    AscendC::SyncAll();

    if (isAic) {
        rb_t30::T30LightCube cube;
        cube.Init();

        EncCrossWait(kFlagAivReady);
        EncTrace(traceGm, SLOT_AIC_POST_WAIT1_NTT, MAGIC_AIC_POST_WAIT1_NTT);
        cube.RunOnce(matCNtt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();
        EncTrace(traceGm, SLOT_AIC_PRE_SET3_NTT, MAGIC_AIC_PRE_SET3_NTT);
        EncCrossSet(kFlagAicDone);

        EncCrossWait(kFlagGate);
        EncTrace(traceGm, SLOT_AIC_POST_WAIT4, MAGIC_AIC_POST_WAIT4);

        EncCrossWait(kFlagAivReady);
        EncTrace(traceGm, SLOT_AIC_POST_WAIT1_INTT, MAGIC_AIC_POST_WAIT1_INTT);
        cube.RunOnce(matCIntt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();
        EncTrace(traceGm, SLOT_AIC_PRE_SET3_INTT, MAGIC_AIC_PRE_SET3_INTT);
        EncCrossSet(kFlagAicDone);
    } else {
        if (subIdx == 0) {
            EncTrace(traceGm, SLOT_AIV0_PRE_SET1_NTT, MAGIC_AIV0_PRE_SET1_NTT);
        } else {
            EncTrace(traceGm, SLOT_AIV1_PRE_SET1_NTT, MAGIC_AIV1_PRE_SET1_NTT);
        }
        EncCrossSet(kFlagAivReady);

        EncCrossWait(kFlagAicDone);
        if (subIdx == 0) {
            EncTrace(traceGm, SLOT_AIV0_POST_WAIT3_NTT, MAGIC_AIV0_POST_WAIT3_NTT);
            rb_t30::ForwardNttY(ws);
            EncTrace(traceGm, SLOT_AIV0_YHAT_DONE, MAGIC_AIV0_YHAT_DONE);
            rb_t30::AssembleUvNttDomain(ws);
            EncTrace(traceGm, SLOT_AIV0_MUL_DONE, MAGIC_AIV0_MUL_DONE);
        } else {
            EncTrace(traceGm, SLOT_AIV1_POST_WAIT3_NTT, MAGIC_AIV1_POST_WAIT3_NTT);
        }

        if (subIdx == 0) {
            EncTrace(traceGm, SLOT_AIV0_PRE_SET4, MAGIC_AIV0_PRE_SET4);
        } else {
            EncTrace(traceGm, SLOT_AIV1_PRE_SET4, MAGIC_AIV1_PRE_SET4);
        }
        EncCrossSet(kFlagGate);

        if (subIdx == 0) {
            EncTrace(traceGm, SLOT_AIV0_PRE_SET1_INTT, MAGIC_AIV0_PRE_SET1_INTT);
        } else {
            EncTrace(traceGm, SLOT_AIV1_PRE_SET1_INTT, MAGIC_AIV1_PRE_SET1_INTT);
        }
        EncCrossSet(kFlagAivReady);

        EncCrossWait(kFlagAicDone);
        if (subIdx == 0) {
            EncTrace(traceGm, SLOT_AIV0_POST_WAIT3_INTT, MAGIC_AIV0_POST_WAIT3_INTT);
            rb_t30::InttAddNoiseUv(ws);
            EncTrace(traceGm, SLOT_AIV0_UV_DONE, MAGIC_AIV0_UV_DONE);
            rb_t30::PackCiphertext(uGm, vGm, cWs, cOut);
            EncTrace(traceGm, SLOT_AIV0_PACK_DONE, MAGIC_AIV0_PACK_DONE);

            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            EncTrace(traceGm, SLOT_AIV1_POST_WAIT3_INTT, MAGIC_AIV1_POST_WAIT3_INTT);
        }
    }
}
