/**
 * @file mmad_custom.cpp
 * @brief pass-toy-encrypt-fsm-l18-skel1：同核串 NTT 1/3 → GATE 4↔8 → INTT 1/3 + TRACE。
 *
 * 图谱：Q-TOY-SIM-2LAUNCH-HANG（GT-20260903-7）；握手骨架同 GT-3..6。
 *
 * 核内序（FULL / 默认）：
 *   桩前缀(μ) → NTT：SET(1)/WAIT(1)/MMAD轻/SET(3)/WAIT(3)
 *   → 桩 at_jp（本地 PIPE，禁真大矩阵）
 *   → GATE：双AIV SET(4)/AIC WAIT(4)/SET(8)/双AIV WAIT(8)
 *   → INTT：再 SET(1)/WAIT(1)/MMAD轻/SET(3)/WAIT(3)
 *   → 完成标记
 *
 * 背景=模仿 F-SIM-LAUNCH（生产 Encrypt 2 Host launch）；结论=tiling.phase 两段：
 *   NTT_ONLY → 跑完 NTT 即 return；GATE_INTT_ONLY → 跳过 NTT 直进 GATE→INTT。
 * 未采用=同核 fused 当唯一加压。
 *
 * TRACE：每槽 8×int32=32B；AIV0 / AIC 在原位点 mark；Host 按 stride 打印。
 * 禁令：无 SyncAll@AIC-Wait；无 SoftSync；禁 Duplicate(int8)；禁 INTT flag 5/7；禁真 SHAKE。
 * 不对算法正确性；验收只认 SIM 两段 sync done + TRACE 可见。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

/**
 * CrossCore FSM 字面量：与 Encrypt l18 同构。
 * NTT/INTT 均用 1/3；GATE 用 4/8；ST_AIC_MMAD=2 保留编号，本玩具不 Wait。
 */
enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1, /**< NTT/INTT：AIV→AIC */
    ST_AIC_MMAD = 2,  /**< 保留；禁 Wait(2) */
    ST_AIV_PACK = 3,  /**< NTT/INTT：AIC→AIV */
    ST_GATE_AIV = 4,  /**< GATE：AIV→AIC */
    ST_GATE_AIC = 8,  /**< GATE：AIC→AIV */
};

/**
 * 等待对端 CrossCore 置位；通道 <2, PIPE_MTE2> 与 F-CC-CHANNEL 一致。
 * @param st 期望 flag（1/3/4/8）
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
 * TRACE 写槽（GT-20260903-4）：LocalTensor 赋值后 DataCopy 到 GM；禁止 `__gm__ int32` 直写。
 *
 * 背景：GT-3 上 AIC 标量写 TRACE 槽 Host 假空（J-FAIL-AIC-SCALAR-TRACE）；本刀验 Q-TOY-TRACE-DATACOPY。
 * 布局：逻辑槽 slot → GM 偏移 slot*8 个 int32（32B 对齐块）；块首元素置 1。
 * 路径：
 *   - AIV0：VECOUT UB 上 Duplicate(0)+SetValue(0,1) → DataCopy 到槽块
 *   - AIC：Cube 无 UB Duplicate；从 ws+TRACE_ONES（Host 预填全 1）DataCopy→A1→槽块
 * 未采用：标量 `trace[slot]=1`；AIC Wait 期间 SyncAll。
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

    // 目的：本逻辑槽的 32B 块（首元素为有效标记）
    AscendC::GlobalTensor<int32_t> dstGm;
    dstGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(traceGm) + slotOffInts, kAlign);

    AscendC::TPipe pipe;
    if (aic) {
        // Cube：L1(A1) 中转；源为 Host 预填的全 1 模板（禁标量写 GM / 禁靠 UB Duplicate）
        AscendC::GlobalTensor<int32_t> onesGm;
        onesGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + tiling::TRACE_ONES), kAlign);

        AscendC::TQue<AscendC::TPosition::A1, 1> a1Q;
        pipe.InitBuffer(a1Q, 1, kAlign * sizeof(int32_t));
        AscendC::LocalTensor<int32_t> t = a1Q.AllocTensor<int32_t>();
        // GM(ones) → L1
        AscendC::DataCopy(t, onesGm, kAlign);
        a1Q.EnQue(t);
        t = a1Q.DeQue<int32_t>();
        // L1 → GM(槽块)；count*4=32B，满足 DataCopy 对齐
        AscendC::DataCopy(dstGm, t, kAlign);
        a1Q.FreeTensor(t);
        AscendC::PipeBarrier<PIPE_ALL>();
    } else {
        // Vector：UB(VECOUT) 写 1 再搬到槽块（与 Decrypt「禁标量写 GM」同模式）
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
 * AIC：NTT 段 WAIT(1)→极轻 MMAD→SET(3)；禁 SyncAll。
 * @param ws / trace / aic / subBlockID 同入口
 */
__aicore__ inline void AicRunNtt(GM_ADDR ws, GM_ADDR trace, const bool aic, int32_t subBlockID)
{
    FsmState st = ST_AIV_SPLIT;
    FsmWait(st);
    ToyTraceMark(trace, ws, TR_AIC_WAIT1_NTT, aic, subBlockID);

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
 * AIC：GATE WAIT(4)→SET(8) 再 INTT WAIT(1)→MMAD→SET(3)；禁 flag 5/7。
 */
__aicore__ inline void AicRunGateIntt(GM_ADDR ws, GM_ADDR trace, const bool aic, int32_t subBlockID)
{
    FsmState st = ST_GATE_AIV;
    FsmWait(st);
    ToyTraceMark(trace, ws, TR_AIC_WAIT4_GATE, aic, subBlockID);
    st = ST_GATE_AIC;
    FsmSet(st);

    // INTT：复用 flag 1/3（禁 5/7）
    st = ST_AIV_SPLIT;
    FsmWait(st);
    ToyTraceMark(trace, ws, TR_AIC_WAIT1_INTT, aic, subBlockID);

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
 * AIV：μ 桩前缀 + NTT SET(1)/WAIT(3)。NTT_ONLY 在此后 return（不做完成标记）。
 */
__aicore__ inline void AivRunNtt(GM_ADDR ws, GM_ADDR trace, const bool aic, int32_t subBlockID)
{
    {
        AivStubHashSplit split(subBlockID);
        split.Init(ws);
        split.Process();
        KYBER_PIPE_ALL();
    }
    ToyTraceMark(trace, ws, TR_AIV_MU_STUB, aic, subBlockID);

    {
        FsmState st = ST_AIV_SPLIT;
        AivStubHashSplit split(subBlockID);
        split.Init(ws);
        split.Process();
        KYBER_PIPE_ALL();
        FsmSet(st); // 双 AIV 均 SET(1)
    }
    FsmWait(ST_AIV_PACK);
    ToyTraceMark(trace, ws, TR_AIV_NTT_DONE, aic, subBlockID);
}

/**
 * AIV：at_jp 桩屏障 → GATE 4↔8 → INTT 1/3 → 完成标记。
 * GATE_INTT_ONLY 跳过 NTT/μ，直接从此段起跑。
 */
__aicore__ inline void AivRunGateIntt(GM_ADDR out, GM_ADDR ws, GM_ADDR trace, const bool aic,
                                      int32_t subBlockID)
{
    // 桩 at_jp：仅 PIPE 屏障，无真内积、无 SoftSync、无大矩阵
    KYBER_PIPE_ALL();

    FsmState st = ST_GATE_AIV;
    FsmSet(st);
    st = ST_GATE_AIC;
    FsmWait(st);
    ToyTraceMark(trace, ws, TR_AIV_GATE_DONE, aic, subBlockID);

    // INTT 同构 1/3：再写半片 S0，再握手（复用 1/3；禁 5/7）
    {
        st = ST_AIV_SPLIT;
        AivStubHashSplit split(subBlockID);
        split.Init(ws);
        split.Process();
        KYBER_PIPE_ALL();
        FsmSet(st);
    }
    FsmWait(ST_AIV_PACK);
    ToyTraceMark(trace, ws, TR_AIV_INTT_DONE, aic, subBlockID);

    AivDoneMark mark(subBlockID);
    mark.Init(out);
    mark.Process();
    KYBER_PIPE_ALL();
}

/**
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2，单 block；phase 决定本 launch 跑哪些段。
 * @param out   [out] 完成标记（每 AIV 32B；仅 FULL / GATE_INTT_ONLY 写）
 * @param ws    [in/out] S0/LUT/MAT_C/TRACE_ONES；LUT 与 TRACE_ONES 由 host 预填
 * @param trace [out] TRACE 槽块（每逻辑槽 32B）；Host 预清零
 * @param tiling.phase ToyLaunchPhase（FULL / NTT_ONLY / GATE_INTT_ONLY）
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR ws, GM_ADDR trace, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    // GetSubBlockNum()==1 → AIC；否则 AIV，subBlockIdx 区分 0/1
    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const int32_t phase = tiling.phase;

    /*
     * 背景=模仿 F-SIM-LAUNCH；结论=phase 分支两段 Host launch；
     * 未采用=同核 fused 当唯一加压。AIC/AIV 必须同相，否则 CrossCore 挂死。
     */
    if (aic) {
        // ---- AIC：按 phase 跑 NTT / GATE+INTT；禁 SyncAll@Wait ----
        if (phase != PHASE_GATE_INTT_ONLY) {
            AicRunNtt(ws, trace, aic, subBlockID);
            if (phase == PHASE_NTT_ONLY) {
                return;
            }
        }
        if (phase != PHASE_NTT_ONLY) {
            AicRunGateIntt(ws, trace, aic, subBlockID);
        }
    } else {
        // ---- AIV：FULL=μ+NTT+GATE+INTT；NTT_ONLY 在 NTT 后 return；GATE_INTT 跳过 NTT ----
        if (phase != PHASE_GATE_INTT_ONLY) {
            AivRunNtt(ws, trace, aic, subBlockID);
            if (phase == PHASE_NTT_ONLY) {
                return;
            }
        }
        if (phase != PHASE_NTT_ONLY) {
            AivRunGateIntt(out, ws, trace, aic, subBlockID);
        }
    }
}
