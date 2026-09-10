/**
 * @file kg_ntt_dot_encode_custom.cpp
 * @brief RB-K07：单 MIX 串行两段 —— NTT(ŝ/ê) 后 Â∘ŝ̂+ê̂+BE → ek/dk（Host 2 launch）。
 *
 * L1 kg_prep → sync → 本核 → sync。
 * 握手 flag∈{1,3,4}；禁 SoftSync；AIC Wait 环禁 SyncAll。
 * AIC：单 LightCube 两段 Process（禁双 TPipe，见 RB-T03）。
 * AIV：与 Encaps compute 同形——AIV1 不等 AIV0 数学，Wait(3) 后即可进下一段 Set。
 * NTT 结果经 ComputeSeNtt 的 sNttOut/eNttOut 直写 L2b 的 S_NTT/E_NTT。
 */
#include "kernel_operator.h"
#include "k02_inc/light_cube.hpp"
#include "k02_inc/ntt_device_math.hpp"
#include "k02_inc/tiling.h"
#include "k03_inc/dot_encode_device_math.hpp"
#include "k03_inc/tiling.h"

namespace {

constexpr size_t kBaseL2b = tiling::wssize;

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

} // namespace

extern "C" __global__ __aicore__ void kg_ntt_dot_encode_custom(GM_ADDR out, GM_ADDR ekOut,
                                                               GM_ADDR dkOut, GM_ADDR ws,
                                                               TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    GM_ADDR wsL2a = ws;
    GM_ADDR wsL2b = ws + kBaseL2b;

    GM_ADDR trace1 = wsL2a + tiling::OFF_TRACE;
    GM_ADDR matA1 = wsL2a + tiling::OFF_MAT_A;
    GM_ADDR matB1 = wsL2a + tiling::OFF_MAT_B;
    GM_ADDR matC1 = wsL2a + tiling::OFF_MAT_C;

    GM_ADDR trace2 = wsL2b + tiling_k03::OFF_TRACE;
    GM_ADDR matA2 = wsL2b + tiling_k03::OFF_MAT_A;
    GM_ADDR matB2 = wsL2b + tiling_k03::OFF_MAT_B;
    GM_ADDR matC2 = wsL2b + tiling_k03::OFF_MAT_C;

    GM_ADDR sNttToL2b = wsL2b + tiling_k03::OFF_S_NTT;
    GM_ADDR eNttToL2b = wsL2b + tiling_k03::OFF_E_NTT;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (isAic) {
        rb_k02::LightCube cube;
        cube.Init();

        CrossWait(tiling::kFlagGate);
        TraceMark(trace1, tiling::SLOT_AIC_POST_WAIT4, tiling::MAGIC_AIC_POST_WAIT4);
        CrossWait(tiling::kFlagAivReady);
        TraceMark(trace1, tiling::SLOT_AIC_POST_WAIT1, tiling::MAGIC_AIC_POST_WAIT1);
        cube.Process(matC1, matA1, matB1);
        AscendC::PipeBarrier<PIPE_ALL>();
        TraceMark(trace1, tiling::SLOT_AIC_PRE_SET3, tiling::MAGIC_AIC_PRE_SET3);
        CrossSet(tiling::kFlagAicDone);

        CrossWait(tiling::kFlagGate);
        TraceMark(trace2, tiling_k03::SLOT_AIC_POST_WAIT4, tiling_k03::MAGIC_AIC_POST_WAIT4);
        CrossWait(tiling::kFlagAivReady);
        TraceMark(trace2, tiling_k03::SLOT_AIC_POST_WAIT1, tiling_k03::MAGIC_AIC_POST_WAIT1);
        cube.Process(matC2, matA2, matB2);
        AscendC::PipeBarrier<PIPE_ALL>();
        TraceMark(trace2, tiling_k03::SLOT_AIC_PRE_SET3, tiling_k03::MAGIC_AIC_PRE_SET3);
        CrossSet(tiling::kFlagAicDone);
    } else {
        if (subIdx == 0) {
            TraceMark(trace1, tiling::SLOT_AIV0_PRE_SET4, tiling::MAGIC_AIV0_PRE_SET4);
        } else {
            TraceMark(trace1, tiling::SLOT_AIV1_PRE_SET4, tiling::MAGIC_AIV1_PRE_SET4);
        }
        CrossSet(tiling::kFlagGate);

        if (subIdx == 0) {
            TraceMark(trace1, tiling::SLOT_AIV0_PRE_SET1, tiling::MAGIC_AIV0_PRE_SET1);
        } else {
            TraceMark(trace1, tiling::SLOT_AIV1_PRE_SET1, tiling::MAGIC_AIV1_PRE_SET1);
        }
        CrossSet(tiling::kFlagAivReady);

        CrossWait(tiling::kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(trace1, tiling::SLOT_AIV0_POST_WAIT3, tiling::MAGIC_AIV0_POST_WAIT3);
            rb_k02::ComputeSeNtt(wsL2a, sNttToL2b, eNttToL2b);
            TraceMark(trace1, tiling::SLOT_AIV0_DONE, tiling::MAGIC_AIV0_DONE);
        } else {
            TraceMark(trace1, tiling::SLOT_AIV1_POST_WAIT3, tiling::MAGIC_AIV1_POST_WAIT3);
        }

        if (subIdx == 0) {
            TraceMark(trace2, tiling_k03::SLOT_AIV0_PRE_SET4, tiling_k03::MAGIC_AIV0_PRE_SET4);
        } else {
            TraceMark(trace2, tiling_k03::SLOT_AIV1_PRE_SET4, tiling_k03::MAGIC_AIV1_PRE_SET4);
        }
        CrossSet(tiling::kFlagGate);

        if (subIdx == 0) {
            TraceMark(trace2, tiling_k03::SLOT_AIV0_PRE_SET1, tiling_k03::MAGIC_AIV0_PRE_SET1);
        } else {
            TraceMark(trace2, tiling_k03::SLOT_AIV1_PRE_SET1, tiling_k03::MAGIC_AIV1_PRE_SET1);
        }
        CrossSet(tiling::kFlagAivReady);

        CrossWait(tiling::kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(trace2, tiling_k03::SLOT_AIV0_POST_WAIT3, tiling_k03::MAGIC_AIV0_POST_WAIT3);
            rb_k03::ComputeDotEncode(wsL2b, ekOut, dkOut);
            TraceMark(trace2, tiling_k03::SLOT_AIV0_DONE, tiling_k03::MAGIC_AIV0_DONE);
            AscendC::PipeBarrier<PIPE_ALL>();
            *reinterpret_cast<__gm__ uint32_t *>(out) = tiling_k03::MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            TraceMark(trace2, tiling_k03::SLOT_AIV1_POST_WAIT3, tiling_k03::MAGIC_AIV1_POST_WAIT3);
        }
    }
}
