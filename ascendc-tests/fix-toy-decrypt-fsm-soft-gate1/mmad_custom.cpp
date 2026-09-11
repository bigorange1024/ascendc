/**
 * @file mmad_custom.cpp
 * @brief fix-toy-decrypt-fsm-soft-gate1：SoftSyncArrive(slot0) 后再一轮 GATE 4↔8。
 *
 * 图谱：Q-TOY-SOFT-GATE；承接 F-TOY-SOFTSYNC-SIM-PASS / F-SOFTSYNC-ARRIVE / F-FSM-GATES。
 * 形态：KERNEL_TYPE_MIX_AIC_1_2；通道 <2, PIPE_MTE2>；flag 仅 4 与 8。
 *
 *   双 AIV：SoftSyncArrive(slot=0) → SET(4) → WAIT(8) → DataCopy TRACE
 *   AIC：WAIT(4) → 极轻 MMAD 16×32×32 → SET(8)；WAIT 期间禁 SyncAll
 *
 * 本刀无 NTT/INTT、无第二轮 GATE。不对算法正确性；验收只认 SIM。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

/**
 * CrossCore FSM 字面量：与 Decrypt prep 段末 / Encrypt GATE 同构。
 * 本刀仅用 GATE 4/8；禁止 1/3/5/7（无 NTT/INTT）。
 */
enum FsmState : uint16_t {
    ST_GATE_AIV = 4, /**< GATE：AIV→AIC */
    ST_GATE_AIC = 8, /**< GATE：AIC→AIV */
};

/**
 * 等待对端 CrossCore 置位；通道 <2, PIPE_MTE2>。
 * @param st 期望 flag（本刀仅 4 或 8）
 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * 向对端广播 FSM 完成。
 * @param st 要置位的 flag（本刀仅 4 或 8）
 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2，单 block。
 * @param out         [out] TRACE/完成标记（每槽 32B；slot0=AIV0，slot1=AIV1）
 * @param ws          [in/out] S0/LUT/MAT_C；S0+LUT 由 Host 预填
 * @param softSyncGm  [in/out] SoftSync 哨兵；Host 已 H2D 清零；本刀只用 slot0
 * @param tiling      占位（本玩具无分段）
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR ws, GM_ADDR softSyncGm,
                                                  TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    // GetSubBlockNum()==1 → AIC；否则 AIV，subBlockIdx 区分 0/1
    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    FsmState st;

    if (aic) {
        // ---- AIC：GATE WAIT(4) → 极轻 MMAD → SET(8)；WAIT 期间禁 SyncAll ----
        // 背景：J-FAIL-SYNCALL-ON-AIC-WAIT —— AIC Wait(4) 期间若 SyncAll 会死锁。
        // 结论：本段只 WaitFlag + Cube + SetFlag，绝不 SyncAll。
        // 未采用：Wait 期间插 SyncAll；第二轮 GATE；NTT/INTT flag 1/3/5/7。
        st = ST_GATE_AIV;
        FsmWait(st);

        AicMmad mmad(static_cast<uint16_t>(tiling::kRows), static_cast<uint16_t>(tiling::kDim),
                     static_cast<uint16_t>(tiling::kCols));
        mmad.Init();
        mmad.Process(ws + tiling::MAT_C, ws + tiling::S0, ws + tiling::LUT);
        KYBER_PIPE_ALL();

        st = ST_GATE_AIC;
        FsmSet(st);
        return;
    }

    // ---- AIV：生产同构 SoftSyncArrive(slot0) ----
    // 背景：隔离 Decrypt AIV1 忙等（F-SOFTSYNC-ARRIVE）；DGT-1 已证 SoftSync 单段 SIM 可过。
    // 结论：本刀 SoftSync 后叠一轮 GATE 4↔8，测 SoftSync+GATE 融合是否挂死。
    // 未采用：用 CrossCore / SyncAll 代替 SoftSync；第二轮 GATE；NTT/INTT。
    SoftSyncArrive(softSyncGm, tiling::kSoftSyncSlot, subBlockID);
    KYBER_PIPE_ALL();

    // ---- GATE 4↔8：双 AIV SET(4)；等 AIC SET(8) ----
    // 背景：同构 Decrypt prep 段末握手（F-FSM-GATES）；J-TWO-GATE-DIFF 未证，本刀只测一轮。
    // 结论：SoftSync 汇合后再 SET(4)/WAIT(8)，看 SIM 是否 SynchronizeStream 挂死。
    // 未采用：第二轮 GATE；AIC Wait 中 SyncAll；用 SoftSync 代替 CrossCore GATE。
    st = ST_GATE_AIV;
    FsmSet(st);
    st = ST_GATE_AIC;
    FsmWait(st);

    // SoftSync+GATE 均通过：AIV0/AIV1 各写一槽完成标记（GT-4 DataCopy；禁 Duplicate(int8)）
    {
        AivDoneMark mark(subBlockID);
        mark.Init(out);
        mark.Process();
        KYBER_PIPE_ALL();
    }
}
