/**
 * @file mmad_custom.cpp
 * @brief pass-toy-encrypt-fsm-gate-intt1：GATE CrossCore 4↔8 后再跑 INTT 同构 flag 1/3。
 *
 * 图谱：Q-TOY-GATE-INTT；承接 F-TOY-NTT1-SIM-PASS / F-INTT-FLAGS / F-WITH-GATE-PASS。
 * 核内序（跳过可选短 NTT，保证 1/3 生命周期干净留给 INTT）：
 *   桩前缀 → 双 AIV SET(4) → AIC WAIT(4) → AIC SET(8) → 双 AIV WAIT(8)
 *   → INTT：AIV SET(1) → AIC WAIT(1)+极轻 MMAD → AIC SET(3) → AIV WAIT(3) → 完成标记
 *
 * 禁令：无 SyncAll@AIC-Wait；无 SoftSync；INTT 禁 flag 5/7；禁 Duplicate(int8)；禁真 SHAKE。
 * 不对算法正确性；验收只认 SIM 进程正常结束。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

/**
 * CrossCore FSM 字面量：与 Encrypt l18 同构。
 * GATE 用 4/8；INTT 复用 1/3（F-INTT-FLAGS）；禁止 5/7（J-FAIL-INTT-FLAGS-57）。
 * ST_AIC_MMAD=2 保留编号，本玩具不 Wait。
 */
enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1, /**< INTT：AIV→AIC */
    ST_AIC_MMAD = 2,  /**< 保留；INTT 注释禁 Wait(2) */
    ST_AIV_PACK = 3,  /**< INTT：AIC→AIV */
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
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2，单 block、单 Host launch。
 * @param out [out] 完成标记（每 AIV 32B）
 * @param ws  [in/out] S0/LUT/MAT_C；LUT 由 host 预填单位阵
 * @param tiling 占位（本玩具无分段）
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR ws, TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    // GetSubBlockNum()==1 → AIC；否则 AIV，subBlockIdx 区分 0/1
    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    FsmState st;

    if (aic) {
        // ---- 段 GATE：等双 AIV SET(4) → SET(8)；本段无 Cube、无 SyncAll ----
        // 背景：AIC 仍 Wait 时禁止对 AIV SyncAll（D-NO-SYNCALL-WHILE-AIC-WAIT）
        st = ST_GATE_AIV;
        FsmWait(st);
        st = ST_GATE_AIC;
        FsmSet(st);

        // ---- 段 INTT：复用 flag 1/3（禁 5/7）；WAIT(1)→极轻 MMAD→SET(3) ----
        st = ST_AIV_SPLIT;
        FsmWait(st);

        AicMmad mmad(static_cast<uint16_t>(tiling::kRows), static_cast<uint16_t>(tiling::kDim),
                     static_cast<uint16_t>(tiling::kCols));
        mmad.Init();
        mmad.Process(ws + tiling::MAT_C, ws + tiling::S0, ws + tiling::LUT);
        KYBER_PIPE_ALL();

        st = ST_AIV_PACK;
        FsmSet(st);
    } else {
        // ---- AIV：桩前缀（禁 SHAKE；SetValue 填 int8，禁 Duplicate(int8)）----
        {
            AivStubHashSplit split(subBlockID);
            split.Init(ws);
            split.Process();
            KYBER_PIPE_ALL();
        }
        // 本刀跳过可选短 NTT(1/3)：留给后段 INTT 独占 1/3，避免同核双用未清生命周期。
        // 极轻「at_jp」替代：仅 PIPE 屏障，无真内积、无 SoftSync。
        KYBER_PIPE_ALL();

        // ---- GATE 4↔8：双 AIV SET(4)；等 AIC SET(8) ----
        st = ST_GATE_AIV;
        FsmSet(st);
        st = ST_GATE_AIC;
        FsmWait(st);

        // ---- INTT 同构 1/3：再写半片 S0 作「左矩阵就绪」意图，再握手 ----
        {
            st = ST_AIV_SPLIT;
            AivStubHashSplit split(subBlockID);
            split.Init(ws);
            split.Process();
            KYBER_PIPE_ALL();
            FsmSet(st); // 双 AIV 均 SET(1)
        }

        st = ST_AIV_PACK;
        FsmWait(st);

        AivDoneMark mark(subBlockID);
        mark.Init(out);
        mark.Process();
        KYBER_PIPE_ALL();
    }
}
