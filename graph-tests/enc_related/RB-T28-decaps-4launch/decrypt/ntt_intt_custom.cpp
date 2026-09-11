/**
 * @file ntt_intt_custom.cpp
 * @brief RB-T28 Launch2：单 MIX 串行两段 —— NTT+su_dot 后 INTT+extract → m'。
 *
 * 背景：T26 将 NTT/INTT 拆成两 launch（反卡死 §5.1）；D08 已证明同核串行复用
 * flag {1,3} 各跑完整一轮可行。本刀在 **统一 dec workspace** 上融合两段，
 * Host Decaps 由 5 launch 压到 4（dec_prep + 本核 + enc_prep + enc_compute）。
 *
 * 结论：段1=原 ntt_custom；段2=原 intt_custom；单 LightCube 两段 Process；
 * AIV1 不等 AIV0 数学。未采用：flag 5/7、双 TPipe、SoftSync、同 Wait 环 SyncAll。
 */
#include "kernel_operator.h"
#include "decrypt/light_cube.hpp"
#include "decrypt/ntt_device_math.hpp"
#include "decrypt/intt_device_math.hpp"
#include "decrypt/tiling.h"

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
 * 融合 MIX：段1 û←NTT(u)、ŵ←⟨ŝ,û⟩；段2 w←INTT(ŵ)、m←extract(v−w)。
 * @param out  [out] MAGIC_OUT_OK（AIV0，段2 完成）
 * @param mOut [out] m'[32]
 * @param ws   [in/out] 统一 dec_tiling workspace（prep 已写 ŝ/u/v/ζ/γ/mat）
 * @param tiling 占位
 */
extern "C" __global__ __aicore__ void dec_ntt_intt_custom(GM_ADDR out, GM_ADDR mOut, GM_ADDR ws,
                                                          DecTilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    using namespace dec_tiling;
    GM_ADDR traceGm = ws + OFF_TRACE;
    GM_ADDR matA = ws + OFF_MAT_A;
    GM_ADDR matB = ws + OFF_MAT_B;
    GM_ADDR matCNtt = ws + OFF_MAT_C_NTT;
    GM_ADDR matCIntt = ws + OFF_MAT_C_INTT;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (isAic) {
        rb_t25::LightCube cube;
        cube.Init();

        // ---- 段1 NTT 握手（flag 1/3）----
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1_NTT, MAGIC_AIC_POST_WAIT1_NTT);
        cube.Process(matCNtt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();
        TraceMark(traceGm, SLOT_AIC_PRE_SET3_NTT, MAGIC_AIC_PRE_SET3_NTT);
        CrossSet(kFlagAicDone);

        // ---- 段2 INTT 握手（复用同组 flag；D08 已证完整一轮后可复用）----
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
            rb_t25::ComputeNttAndSuDot(ws);
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
            rb_t25::ComputeInttAndExtract(ws, mOut);
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
