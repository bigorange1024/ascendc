/**
 * @file mmad_custom.cpp
 * @brief E18 stub：单 launch MIX，复现 l18 CrossCore 序（非业务）。
 *
 * 同步序（对照 ENCRYPT_GAP G1+G2 / l18 文件头 FSM，禁抄 Encrypt 实现）：
 *   伪 NTT：AIV SET(1) ↔ AIC Wait(1)；AIC SET(3) ↔ AIV Wait(3)
 *   GATE：  双 AIV SET(4) ↔ AIC Wait(4)；AIC SET(8) ↔ 双 AIV Wait(8)
 *   伪 INTT：AIV SET(5) ↔ AIC Wait(5)；AIC SET(6) ↔ AIV Wait(6)  ← 相对 E17 唯一差分
 *
 * 背景：图谱 D-exp-e18 — 相对 E17 单因子：INTT 从复用 1/3 改为独立 5/6。
 * 结论：本核只做 CrossCore 握手与三位数字 TRACE；无 Cube/NTT/INTT/at_jp。
 * 未采用：复用 1/3（E17）；OMIT 对照；真业务。
 *
 * CrossCore：modeId=2、PIPE_MTE2；flag ∈ {1,3,4,5,6,8}；Wait 路径禁止 SyncAll。
 */
#include "kernel_operator.h"
#include "tiling.h"

/** CrossCore flag：对齐 l18 FSM 号（stub 语义，非业务状态机实现）。 */
enum FsmState : uint16_t {
    ST_NTT_AIV_SPLIT = 1, /**< AIV→AIC：伪 Stage1 完成 / 释放伪 MMAD */
    ST_NTT_AIV_PACK = 3,  /**< AIC→AIV：伪 Stage2 完成 / 放行 Pack */
    ST_IP_AIV_DONE = 4,   /**< 双 AIV→AIC：伪内积齐（GATE 入） */
    ST_INTT_AIV_SPLIT = 5, /**< AIV→AIC：伪 INTT 入（独立 5；非复用 1） */
    ST_INTT_AIV_PACK = 6,  /**< AIC→AIV：伪 INTT 出（独立 6；非复用 3） */
    ST_AT_JP_GATE = 8,    /**< AIC→双 AIV：放行伪 INTT */
};

/** 设备侧 TRACE：只打三位十进制数字（见 TRACE.md）。 */
__aicore__ inline void TraceDigit(int code)
{
    AscendC::printf("%d\n", code);
}

/**
 * 等待对端 CrossCore。
 * 前置：禁止在 Wait 期间调用 SyncAll（cannbot CrossCore 纪律）。
 * @param st flagId（1/3/4/5/6/8）
 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * 置位 CrossCore。
 * @param st flagId（1/3/4/5/6/8）
 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * AIV0 写极简 magic（证明全序跑完）；AIV1 / AIC 不写。
 * UB 填常量后 DataCopy → GM（对齐 32B；64B=2 block）。
 * @param out GM 输出 64B
 */
__aicore__ inline void WriteMagic(GM_ADDR out)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    pipe.InitBuffer(outQ, 1, tiling::kOutBytes);
    AscendC::LocalTensor<uint8_t> ub = outQ.AllocTensor<uint8_t>();
    // 写 8B 前缀 "E18TOY01"
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
 * MIX kernel：1 AIC + 2 AIV；单 launch 跑完整 stub 序。
 * @param out [out] AIV0 写 magic
 * @param src [in]  占位（未用）
 * @param ws  [in/out] 占位 workspace（未用）
 * @param tiling 占位（本 toy 无 phase）
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    (void)src;
    (void)ws;
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (aic) {
        // ========== 伪 NTT：Wait(1) →（无 Cube）→ Set(3) ==========
        TraceDigit(400); // AIC 入口 / 将 Wait(1) NTT
        FsmWait(ST_NTT_AIV_SPLIT);
        TraceDigit(401); // Wait(1) 返回；stub 跳过 MMAD
        FsmSet(ST_NTT_AIV_PACK);
        TraceDigit(403); // 已 Set(3)

        // ========== GATE：Wait(4) → Set(8) ==========
        TraceDigit(410); // 将 Wait(4)
        FsmWait(ST_IP_AIV_DONE);
        TraceDigit(411); // Wait(4) 返回
        FsmSet(ST_AT_JP_GATE);
        TraceDigit(418); // 已 Set(8)

        // ========== 伪 INTT：独立 Wait(5)/Set(6)（相对 E17：非复用 1/3）==========
        TraceDigit(420); // 将 Wait(5) INTT 独立
        FsmWait(ST_INTT_AIV_SPLIT);
        TraceDigit(421); // Wait(5) 返回；stub 跳过 INTT MMAD
        FsmSet(ST_INTT_AIV_PACK);
        TraceDigit(423); // 已 Set(6)；AIC 全序完成
    } else {
        // ========== 伪 NTT：双 AIV Set(1) → Wait(3) ==========
        // 入口号：AIV0=500 / AIV1=510
        if (subBlockID == 0) {
            TraceDigit(500);
        } else {
            TraceDigit(510);
        }
        FsmSet(ST_NTT_AIV_SPLIT);
        if (subBlockID == 0) {
            TraceDigit(501);
        } else {
            TraceDigit(511);
        }
        FsmWait(ST_NTT_AIV_PACK);
        if (subBlockID == 0) {
            TraceDigit(503);
        } else {
            TraceDigit(513);
        }

        // ========== GATE：双 AIV 均 Set(4) → Wait(8) ==========
        // 背景：禁止仅 subBlock0 SET(4)，否则 AIC 可能提前进 INTT（对齐 l18 注释纪律）
        FsmSet(ST_IP_AIV_DONE);
        if (subBlockID == 0) {
            TraceDigit(504);
        } else {
            TraceDigit(514);
        }
        FsmWait(ST_AT_JP_GATE);
        if (subBlockID == 0) {
            TraceDigit(508);
        } else {
            TraceDigit(518);
        }

        // ========== 伪 INTT：独立 Set(5) → Wait(6)（相对 E17 唯一差分）==========
        FsmSet(ST_INTT_AIV_SPLIT);
        if (subBlockID == 0) {
            TraceDigit(521);
        } else {
            TraceDigit(531);
        }
        FsmWait(ST_INTT_AIV_PACK);
        if (subBlockID == 0) {
            TraceDigit(523);
        } else {
            TraceDigit(533);
        }

        // 仅 AIV0 写 magic，证明全序可达
        if (subBlockID == 0) {
            WriteMagic(out);
        }
    }
}
