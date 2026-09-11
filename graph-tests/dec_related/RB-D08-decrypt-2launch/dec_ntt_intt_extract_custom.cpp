/**
 * @file dec_ntt_intt_extract_custom.cpp
 * @brief RB-D08：单 MIX 串行两段 —— NTT+su_dot 后 INTT+extract → m（Host 2 launch）。
 *
 * L1 dec_prep → sync → 本核 → sync。
 * 段1=原 D02；段2=原 D03。复用 flag {1,3,4} 各跑完整一轮。
 * AIC：单 LightCube 两段 Process（禁双 TPipe）。
 * AIV：与 Encaps/K07 同形——AIV1 不等 AIV0 数学；ŵ 由段1 直写段2 OFF_W_HAT。
 * 禁 SoftSync；AIC Wait 环禁 SyncAll。
 */
#include "kernel_operator.h"
#include "d02_inc/light_cube.hpp"
#include "d02_inc/ntt_dot_device_math.hpp"
#include "d02_inc/tiling.h"
#include "d03_inc/intt_extract_device_math.hpp"
#include "d03_inc/tiling.h"

namespace {

// DataCopy GM 须 32B 对齐；d02 wssize=17976≡24(mod32)，须垫到 32B
constexpr size_t kBaseL2b = (tiling::wssize + 31u) & ~size_t(31u);

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

/**
 * 融合 MIX：段1 NTT+dot → 段2 INTT+extract。
 * @param out    段2 完成魔数（AIV0）
 * @param mOut   明文 m[32]
 * @param ws     [段1 d02::wssize | 段2 tiling_d03::wssize]
 * @param tiling 占位
 */
extern "C" __global__ __aicore__ void dec_ntt_intt_extract_custom(GM_ADDR out, GM_ADDR mOut,
                                                                  GM_ADDR ws, TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    GM_ADDR ws1 = ws;
    GM_ADDR ws2 = ws + kBaseL2b;

    GM_ADDR tr1 = ws1 + tiling::OFF_TRACE;
    GM_ADDR matA1 = ws1 + tiling::OFF_MAT_A;
    GM_ADDR matB1 = ws1 + tiling::OFF_MAT_B;
    GM_ADDR matC1 = ws1 + tiling::OFF_MAT_C;

    GM_ADDR tr2 = ws2 + tiling_d03::OFF_TRACE;
    GM_ADDR matA2 = ws2 + tiling_d03::OFF_MAT_A;
    GM_ADDR matB2 = ws2 + tiling_d03::OFF_MAT_B;
    GM_ADDR matC2 = ws2 + tiling_d03::OFF_MAT_C;

    // 段1 ŵ 直写段2 期望槽；û 仍落段1 OFF_U_HAT（诊断）
    GM_ADDR uHatOut = ws1 + tiling::OFF_U_HAT;
    GM_ADDR wHatToL2b = ws2 + tiling_d03::OFF_W_HAT;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (isAic) {
        rb_d02::LightCube cube;
        cube.Init();

        // ---- 段1 握手 ----
        CrossWait(tiling::kFlagGate);
        TraceMark(tr1, tiling::SLOT_AIC_POST_WAIT4, tiling::MAGIC_AIC_POST_WAIT4);
        CrossWait(tiling::kFlagAivReady);
        TraceMark(tr1, tiling::SLOT_AIC_POST_WAIT1, tiling::MAGIC_AIC_POST_WAIT1);
        cube.Process(matC1, matA1, matB1);
        AscendC::PipeBarrier<PIPE_ALL>();
        TraceMark(tr1, tiling::SLOT_AIC_PRE_SET3, tiling::MAGIC_AIC_PRE_SET3);
        CrossSet(tiling::kFlagAicDone);

        // ---- 段2 握手（复用同组 flag）----
        CrossWait(tiling_d03::kFlagGate);
        TraceMark(tr2, tiling_d03::SLOT_AIC_POST_WAIT4, tiling_d03::MAGIC_AIC_POST_WAIT4);
        CrossWait(tiling_d03::kFlagAivReady);
        TraceMark(tr2, tiling_d03::SLOT_AIC_POST_WAIT1, tiling_d03::MAGIC_AIC_POST_WAIT1);
        cube.Process(matC2, matA2, matB2);
        AscendC::PipeBarrier<PIPE_ALL>();
        TraceMark(tr2, tiling_d03::SLOT_AIC_PRE_SET3, tiling_d03::MAGIC_AIC_PRE_SET3);
        CrossSet(tiling_d03::kFlagAicDone);
    } else {
        // ---- 段1 AIV ----
        if (subIdx == 0) {
            TraceMark(tr1, tiling::SLOT_AIV0_PRE_SET4, tiling::MAGIC_AIV0_PRE_SET4);
        } else {
            TraceMark(tr1, tiling::SLOT_AIV1_PRE_SET4, tiling::MAGIC_AIV1_PRE_SET4);
        }
        CrossSet(tiling::kFlagGate);
        if (subIdx == 0) {
            TraceMark(tr1, tiling::SLOT_AIV0_PRE_SET1, tiling::MAGIC_AIV0_PRE_SET1);
        } else {
            TraceMark(tr1, tiling::SLOT_AIV1_PRE_SET1, tiling::MAGIC_AIV1_PRE_SET1);
        }
        CrossSet(tiling::kFlagAivReady);
        CrossWait(tiling::kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(tr1, tiling::SLOT_AIV0_POST_WAIT3, tiling::MAGIC_AIV0_POST_WAIT3);
            rb_d02::ComputeNttAndSuDot(ws1, uHatOut, wHatToL2b);
            TraceMark(tr1, tiling::SLOT_AIV0_DONE, tiling::MAGIC_AIV0_DONE);
        } else {
            TraceMark(tr1, tiling::SLOT_AIV1_POST_WAIT3, tiling::MAGIC_AIV1_POST_WAIT3);
        }

        // ---- 段2 AIV（AIV1 不等 AIV0 数学，对齐 K07/Encaps）----
        if (subIdx == 0) {
            TraceMark(tr2, tiling_d03::SLOT_AIV0_PRE_SET4, tiling_d03::MAGIC_AIV0_PRE_SET4);
        } else {
            TraceMark(tr2, tiling_d03::SLOT_AIV1_PRE_SET4, tiling_d03::MAGIC_AIV1_PRE_SET4);
        }
        CrossSet(tiling_d03::kFlagGate);
        if (subIdx == 0) {
            TraceMark(tr2, tiling_d03::SLOT_AIV0_PRE_SET1, tiling_d03::MAGIC_AIV0_PRE_SET1);
        } else {
            TraceMark(tr2, tiling_d03::SLOT_AIV1_PRE_SET1, tiling_d03::MAGIC_AIV1_PRE_SET1);
        }
        CrossSet(tiling_d03::kFlagAivReady);
        CrossWait(tiling_d03::kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(tr2, tiling_d03::SLOT_AIV0_POST_WAIT3, tiling_d03::MAGIC_AIV0_POST_WAIT3);
            rb_d03::ComputeInttAndExtract(ws2, mOut);
            TraceMark(tr2, tiling_d03::SLOT_AIV0_DONE, tiling_d03::MAGIC_AIV0_DONE);
            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = tiling_d03::MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            TraceMark(tr2, tiling_d03::SLOT_AIV1_POST_WAIT3, tiling_d03::MAGIC_AIV1_POST_WAIT3);
        }
    }
}
