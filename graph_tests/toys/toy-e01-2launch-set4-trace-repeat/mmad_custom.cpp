/**
 * @file mmad_custom.cpp
 * @brief E01 极简 MIX：L1 几乎空；L2 AIC Wait(4)+TRACE，双 AIV SET(4)+TRACE。
 *
 * 背景：图谱 D-exp-e01 — 新目录验证 2-launch+SET4+数字 TRACE 可连续 8 轮。
 * 结论：本核只做 CrossCore 握手与三位数字 TRACE；禁止 Encrypt/NTT/μ 业务。
 * 未采用：Cube MMAD、GATE 4↔8、SoftSync、PrefixEmbed。
 *
 * OMIT_SET4=1：AIV 故意不 SET(4) → AIC 死等 → SIM 超时 124（对照 F-omit-set4）。
 */
#include "kernel_operator.h"
#include "tiling.h"

#ifndef OMIT_SET4
#define OMIT_SET4 0
#endif

/** CrossCore flag：仅用 4 = AIV→AIC 齐步（L2 入口）。 */
enum FsmState : uint16_t {
    ST_SET4 = 4,
};

/** 设备侧 TRACE：只打三位十进制数字（知识库 §6）。 */
__aicore__ inline void TraceDigit(int code)
{
    AscendC::printf("%d\n", code);
}

/** 等待对端 CrossCore。禁止在 Wait 期间 SyncAll。 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** 置位 CrossCore。 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * AIV0 写极简 magic（证明 L2 跑完）；AIV1 / AIC 不写。
 * UB 填常量后 DataCopy → GM（对齐 32B；64B=2 block）。
 * @param out GM 输出 64B
 */
__aicore__ inline void WriteMagic(GM_ADDR out)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    pipe.InitBuffer(outQ, 1, tiling::kOutBytes);
    AscendC::LocalTensor<uint8_t> ub = outQ.AllocTensor<uint8_t>();
    for (uint32_t i = 0; i < 8; ++i) {
        ub.SetValue(i, static_cast<uint8_t>(tiling::kMagicPrefix[i]));
    }
    ub.SetValue(8, tiling::kMagicMark);
    for (uint32_t i = 9; i < tiling::kOutBytes; ++i) {
        ub.SetValue(i, tiling::kMagicFill);
    }
    outQ.EnQue(ub);
    ub = outQ.DeQue<uint8_t>();
    AscendC::GlobalTensor<uint8_t> outGm;
    outGm.SetGlobalBuffer((__gm__ uint8_t *)out, tiling::kOutBytes);
    AscendC::DataCopy(outGm, ub, tiling::kOutBytes);
    outQ.FreeTensor(ub);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * MIX kernel：1 AIC + 2 AIV；phase=0 L1 stub；phase=1 L2 Wait/SET(4)。
 * @param out [out] L2 写 magic
 * @param src [in]  占位（未用）
 * @param ws  [in/out] 占位 workspace
 * @param tiling phase 选 L1/L2
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    (void)src;
    (void)ws;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const int32_t phase = tiling.phase;

    if (phase == tiling::kPhaseLaunch1) {
        // L1 stub：几乎空，仅 barrier 后返回（无 Wait(4)、无 Cube）
        AscendC::PipeBarrier<PIPE_ALL>();
        return;
    }

    // ========== L2：AIC Wait(4) ↔ 双 AIV SET(4) ==========
    if (aic) {
        TraceDigit(400); // L2 AIC 入口
        TraceDigit(401); // 将 Wait(4)
        FsmWait(ST_SET4);
        TraceDigit(402); // Wait(4) 后 = SET 配对成功
    } else {
        // 双 AIV：入口号 500/510；SET 后 502/512（知识库预留号）
        if (subBlockID == 0) {
            TraceDigit(500);
        } else {
            TraceDigit(510);
        }
#if !OMIT_SET4
        FsmSet(ST_SET4);
        if (subBlockID == 0) {
            TraceDigit(502);
        } else {
            TraceDigit(512);
        }
#else
        // 故障注入：不 SET(4) → AIC 死等
        AscendC::PipeBarrier<PIPE_ALL>();
#endif
        if (subBlockID == 0) {
            WriteMagic(out);
        }
    }
}
