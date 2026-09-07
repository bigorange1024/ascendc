/**
 * @file mmad_custom.cpp
 * @brief pass-toy-decrypt-fsm-fused-skel1：Decrypt 同核融合握手骨架。
 *
 * 图谱：Q-TOY-FUSED-SKEL / Q-HANG-LOCI；对齐生产 fused_entry 序（非 Encrypt 单 GATE）。
 * 形态：KERNEL_TYPE_MIX_AIC_1_2；通道 <2, PIPE_MTE2>。
 *
 * AIV（双核均走握手；仅 AIV0 写 SoftSync / TRACE）：
 *   SoftSyncArrive(slot0) → SET(4)/WAIT(8) → SoftSyncClear(slot0)
 *   → SET(1)/WAIT(3)（NTT）
 *   → SoftSyncArrive(slot1) → SET(4)/WAIT(8) → SoftSyncClear(slot1)
 *   → SET(1)/WAIT(3)（INTT；禁 Wait(2)；禁 flag 5/7）
 *
 * AIC：
 *   WAIT(4)/SET(8)（prep GATE）→ WAIT(1)/轻 MMAD/SET(3)（NTT）
 *   → WAIT(4)/SET(8)（su GATE）→ WAIT(1)/轻 MMAD/SET(3)（INTT）
 *   任一 Wait 期间禁 SyncAll。
 *
 * TRACE：GT-4 式 32B 槽；AIC 用 ones 模板 DataCopy。不对算法正确性；验收只认 SIM。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

/**
 * CrossCore FSM 字面量：与 Decrypt fused_entry / Encrypt NTT·GATE·INTT 同构编号。
 * NTT/INTT 均用 1/3；GATE 用 4/8；禁 Wait(2)；禁 flag 5/7。
 */
enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1, /**< NTT/INTT：AIV→AIC */
    ST_AIC_MMAD = 2,  /**< 保留编号；本玩具从不 Wait(2) */
    ST_AIV_PACK = 3,  /**< NTT/INTT：AIC→AIV */
    ST_GATE_AIV = 4,  /**< GATE：AIV→AIC */
    ST_GATE_AIC = 8,  /**< GATE：AIC→AIV */
};

/**
 * 等待对端 CrossCore 置位；通道 <2, PIPE_MTE2>。
 * @param st 期望 flag（本刀仅 1/3/4/8）
 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * 向对端广播 FSM 完成。
 * @param st 要置位的 flag
 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * TRACE 写槽（GT-4）：LocalTensor 赋值后 DataCopy 到 GM；禁止 `__gm__ int32` 直写。
 *
 * 背景：AIC 标量写 TRACE 槽 Host 假空（J-FAIL-AIC-SCALAR-TRACE）；toy 可用局部 TPipe。
 * 布局：逻辑槽 slot → GM 偏移 slot*8 个 int32（32B 对齐块）；块首元素置 1。
 * 路径：
 *   - AIV0：VECOUT UB Duplicate(0)+SetValue(0,1) → DataCopy 到槽块
 *   - AIC：从 ws+TRACE_ONES（Host 预填全 1）DataCopy→A1→槽块
 * 未采用：标量 `trace[slot]=1`；AIC Wait 期间 SyncAll；改 stable。
 *
 * @param traceGm TRACE GM 基址（可空）
 * @param ws      workspace（AIC 读 TRACE_ONES；AIV 不用）
 * @param slot    逻辑槽 0..7
 * @param aic     true=AIC
 * @param subBlockID AIV 子块号（仅 0 写）
 */
__aicore__ inline void ToyTraceMark(GM_ADDR traceGm, GM_ADDR ws, ToyTraceSlot slot, const bool aic,
                                    int32_t subBlockID)
{
    if (traceGm == nullptr) {
        return;
    }
    if (!aic && subBlockID != 0) {
        return;
    }

    constexpr uint32_t kAlign = static_cast<uint32_t>(tiling::kTraceAlignInts);
    const uint32_t slotOffInts = static_cast<uint32_t>(slot) * kAlign;

    AscendC::GlobalTensor<int32_t> dstGm;
    dstGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(traceGm) + slotOffInts, kAlign);

    AscendC::TPipe pipe;
    if (aic) {
        // Cube：L1(A1) 中转；源为 Host 预填的全 1 模板（禁标量写 GM）
        AscendC::GlobalTensor<int32_t> onesGm;
        onesGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + tiling::TRACE_ONES), kAlign);

        AscendC::TQue<AscendC::TPosition::A1, 1> a1Q;
        pipe.InitBuffer(a1Q, 1, kAlign * sizeof(int32_t));
        AscendC::LocalTensor<int32_t> t = a1Q.AllocTensor<int32_t>();
        AscendC::DataCopy(t, onesGm, kAlign);
        a1Q.EnQue(t);
        t = a1Q.DeQue<int32_t>();
        AscendC::DataCopy(dstGm, t, kAlign);
        a1Q.FreeTensor(t);
        AscendC::PipeBarrier<PIPE_ALL>();
    } else {
        // Vector：UB(VECOUT) 写 1 再搬到槽块
        AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
        pipe.InitBuffer(outQ, 1, kAlign * sizeof(int32_t));
        AscendC::LocalTensor<int32_t> t = outQ.AllocTensor<int32_t>();
        AscendC::Duplicate(t, static_cast<int32_t>(0), kAlign);
        t.SetValue(0, static_cast<int32_t>(1));
        outQ.EnQue(t);
        t = outQ.DeQue<int32_t>();
        AscendC::DataCopy(dstGm, t, kAlign);
        outQ.FreeTensor(t);
        AscendC::PipeBarrier<PIPE_ALL>();
    }
}

/**
 * AIC：一轮 GATE WAIT(4)→SET(8)；其间禁 SyncAll、禁 MMAD。
 * @param markSlot TRACE 槽（prep / su）
 */
__aicore__ inline void AicRunGate(GM_ADDR ws, GM_ADDR trace, ToyTraceSlot markSlot, const bool aic,
                                  int32_t subBlockID)
{
    // 背景：J-FAIL-SYNCALL-ON-AIC-WAIT —— AIC Wait(4) 期间若 SyncAll 会死锁。
    // 结论：本段只 WaitFlag + SetFlag，绝不 SyncAll；GATE 不做 MMAD。
    // 未采用：Wait 期间插 SyncAll；GATE 间夹 MMAD（那是 soft-gate1 的简化，非 fused 序）。
    FsmState st = ST_GATE_AIV;
    FsmWait(st);
    ToyTraceMark(trace, ws, markSlot, aic, subBlockID);
    st = ST_GATE_AIC;
    FsmSet(st);
}

/**
 * AIC：NTT 或 INTT 段 WAIT(1)→极轻 MMAD→SET(3)；禁 Wait(2)；禁 flag 5/7。
 */
__aicore__ inline void AicRunNttOrIntt(GM_ADDR ws, GM_ADDR trace, ToyTraceSlot markSlot, const bool aic,
                                      int32_t subBlockID)
{
    FsmState st = ST_AIV_SPLIT;
    FsmWait(st);
    ToyTraceMark(trace, ws, markSlot, aic, subBlockID);

    {
        AicMmad mmad(static_cast<uint16_t>(tiling::kRows), static_cast<uint16_t>(tiling::kDim),
                     static_cast<uint16_t>(tiling::kCols));
        mmad.Init();
        mmad.Process(ws + tiling::MAT_C, ws + tiling::S0, ws + tiling::LUT);
        KYBER_PIPE_ALL();
    }
    st = ST_AIV_PACK;
    FsmSet(st);
}

/**
 * AIV：SoftSyncArrive(slot) → GATE 4↔8 → SoftSyncClear(slot) → TRACE。
 * 背景：同构 fused_entry prep/su 段末；结论：Clear 后再开 NTT/INTT。
 * 未采用：Encrypt 单 GATE；第二 SoftSync 省略；用 SyncAll 汇合。
 */
__aicore__ inline void AivSoftGateClear(GM_ADDR softSyncGm, GM_ADDR ws, GM_ADDR trace, int32_t slot,
                                        ToyTraceSlot markSlot, const bool aic, int32_t subBlockID)
{
    SoftSyncArrive(softSyncGm, slot, subBlockID);
    KYBER_PIPE_ALL();

    FsmState st = ST_GATE_AIV;
    FsmSet(st);
    st = ST_GATE_AIC;
    FsmWait(st);

    SoftSyncClear(softSyncGm, slot, subBlockID);
    KYBER_PIPE_ALL();

    ToyTraceMark(trace, ws, markSlot, aic, subBlockID);
}

/**
 * AIV：NTT 或 INTT 同构 SET(1)/WAIT(3)；禁 Wait(2)；禁 flag 5/7。
 * 桩：无真 Stage1；S0 由 Host 预填，仅握手。
 */
__aicore__ inline void AivRunNttOrIntt(GM_ADDR ws, GM_ADDR trace, ToyTraceSlot markSlot, const bool aic,
                                      int32_t subBlockID)
{
    (void)subBlockID;
    FsmState st = ST_AIV_SPLIT;
    FsmSet(st); // 双 AIV 均 SET(1)
    FsmWait(ST_AIV_PACK);
    ToyTraceMark(trace, ws, markSlot, aic, subBlockID);
}

/**
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2，单 block，单 launch 全融合序。
 * @param out         [out] 完成标记（每 AIV 32B）
 * @param ws          [in/out] S0/LUT/MAT_C/TRACE_ONES；LUT 与 TRACE_ONES 由 Host 预填
 * @param softSyncGm  [in/out] SoftSync 哨兵；Host 已 H2D 清零；slot0+slot1
 * @param trace       [out] TRACE 槽块（每逻辑槽 32B）；Host 预清零
 * @param tiling      占位（本玩具无分段）
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR ws, GM_ADDR softSyncGm,
                                                  GM_ADDR trace, TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (aic) {
        // ---- AIC：prep GATE → NTT → su GATE → INTT；禁 SyncAll@Wait ----
        // 背景：对齐 fused_entry AIC 两轮 GATE + 两轮 MMAD；结论：GATE 无 MMAD，NTT/INTT 用 1/3。
        // 未采用：soft-gate1 在 GATE 内夹 MMAD；INTT 用 5/7；Wait(2)。
        AicRunGate(ws, trace, TR_AIC_WAIT4_PREP, aic, subBlockID);
        AicRunNttOrIntt(ws, trace, TR_AIC_WAIT1_NTT, aic, subBlockID);
        AicRunGate(ws, trace, TR_AIC_WAIT4_SU, aic, subBlockID);
        AicRunNttOrIntt(ws, trace, TR_AIC_WAIT1_INTT, aic, subBlockID);
        return;
    }

    // ---- AIV：双 SoftSync + 双 GATE + NTT/INTT 1/3（Decrypt fused 序）----
    // 背景：J-TWO-GATE-DIFF / J-PRIMARY-SOFTSYNC 未证；本刀叠全序看 SIM 是否挂死。
    // 结论：严格按 Soft0→GATE→Clear→NTT→Soft1→GATE→Clear→INTT；禁 Encrypt 单 GATE。
    // 未采用：省略第二 SoftSync；INTT Wait(2)/flag 5/7；真 SHAKE/真 NTT 数学。
    AivSoftGateClear(softSyncGm, ws, trace, /*slot=*/0, TR_AIV_SOFT0_GATE, aic, subBlockID);
    AivRunNttOrIntt(ws, trace, TR_AIV_NTT_DONE, aic, subBlockID);
    AivSoftGateClear(softSyncGm, ws, trace, /*slot=*/1, TR_AIV_SOFT1_GATE, aic, subBlockID);
    AivRunNttOrIntt(ws, trace, TR_AIV_INTT_DONE, aic, subBlockID);

    {
        AivDoneMark mark(subBlockID);
        mark.Init(out);
        mark.Process();
        KYBER_PIPE_ALL();
    }
}
