/**
 * @file mmad_custom.cpp
 * @brief E03 Encrypt 形态骨架：L1 采样 stub TRACE；L2 假 NTT/点积/INTT + SET(4)。
 *
 * 背景：图谱 D-exp-e03 — 新目录做出采样→代数阶段顺序可见的 2-launch 骨架。
 * 结论：本核只打三位数字 TRACE + CrossCore SET/Wait(4)；禁止真 SHAKE/NTT/Encrypt 抄码。
 * 未采用：SoftSync（E02 已证极简非必要）、Cube MMAD、GATE alone、OMIT_SET4 发现。
 *
 * TRACE 号段见 TRACE.md（L1 200–203；L2 400/401/402 + AIV 假代数 + 502/512）。
 */
#include "kernel_operator.h"
#include "tiling.h"

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
 * L1 采样形态 stub：仅 TRACE 打点，无真 seed expand / CBD / SHAKE。
 * 仅 AIV0 打点（避免双 AIV 交错难读）；AIC/AIV1 仅 barrier。
 * 顺序：200 入 → 201 假 seed → 202 假 CBD/noise → 203 将返回。
 */
__aicore__ inline void L1SamplingStub(bool aic, int32_t subBlockID)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    if (!aic && subBlockID == 0) {
        TraceDigit(200); // 进入 L1
        TraceDigit(201); // 假 seed expand（无 SHAKE）
        TraceDigit(202); // 假 CBD/noise
        TraceDigit(203); // L1 将返回
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * L2 AIV 代数形态 stub：假 NTT → 假点积 → 假 INTT → SET(4)。
 * AIV0 用 520/530/540/502；AIV1 用 521/531/541/512。
 * @param subBlockID 0 或 1
 * @param out 仅 AIV0 写 magic
 */
__aicore__ inline void L2AlgebraStubAiv(int32_t subBlockID, GM_ADDR out)
{
    if (subBlockID == 0) {
        TraceDigit(500); // AIV0 入
        TraceDigit(520); // 假 NTT
        TraceDigit(530); // 假点积
        TraceDigit(540); // 假 INTT
        FsmSet(ST_SET4);
        TraceDigit(502); // 已 SET(4)
        WriteMagic(out);
    } else {
        TraceDigit(510); // AIV1 入
        TraceDigit(521); // 假 NTT
        TraceDigit(531); // 假点积
        TraceDigit(541); // 假 INTT
        FsmSet(ST_SET4);
        TraceDigit(512); // 已 SET(4)
    }
}

/**
 * MIX kernel：1 AIC + 2 AIV；phase=0 L1 采样 stub；phase=1 L2 代数 stub + Wait/SET(4)。
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
        L1SamplingStub(aic, subBlockID);
        return;
    }

    // ========== L2：AIC Wait(4) ↔ 双 AIV 假代数 + SET(4) ==========
    if (aic) {
        TraceDigit(400); // L2 AIC 入口
        TraceDigit(401); // 将 Wait(4)
        FsmWait(ST_SET4);
        TraceDigit(402); // Wait(4) 后 = SET 配对成功
    } else {
        L2AlgebraStubAiv(subBlockID, out);
    }
}
