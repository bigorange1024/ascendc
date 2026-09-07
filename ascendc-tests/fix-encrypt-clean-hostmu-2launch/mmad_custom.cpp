/**
 * @file mmad_custom.cpp
 * @brief 干净 Encrypt：同一 MIX 入口按 tiling.phase 区分两趟 Host launch。
 *
 * Launch1（phase=0）：prep+NTT 一轮 Cube；**无**设备 μ、**无** Wait(4)。
 * Launch2（phase=1）：skipNtt —
 *   AIC：入口 Wait(4) → Set(8) → Wait(1) MMAD Set(3)（GATE + INTT-like）
 *   AIV：短 at_jp stub（**无 PrefixEmbed**）→ 双 AIV Set(4) → Wait(8) → Cube → magic
 * P1a：L2 写 ws+TRACE 段标记（SET(4) 前/后），供 Host 读出。
 *
 * 背景（ENCRYPT_CLEAN_REWRITE）：μ 只在 Host；SET(4) 可达为第一不变量。
 * 禁止：AIC Wait 期间 SyncAll；自造 SoftSync；真 SHAKE；碎写 GM / 滥 launch。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

/**
 * CrossCore：
 *   flag 1 / 3 — Cube 握手（NTT/INTT-like）
 *   flag 4 / 8 — skipNtt 入口齐步 + GATE（AIV→AIC / AIC→AIV）
 */
enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1,
    ST_AIC_MMAD = 2,    /**< 保留；与 Encrypt 枚举对齐 */
    ST_AIV_PACK = 3,
    ST_IP_AIV_DONE = 4, /**< skipNtt / GATE：AIV 齐步 → AIC Wait(4) */
    ST_AT_JP_GATE = 8,  /**< GATE：AIC 放行 → AIV */
};

/** 等待对端 CrossCore。禁止在 Wait 期间 SyncAll。 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/** 置位 CrossCore。 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * P1a：写 TRACE 槽。AIV 仅 sub0；AIC 可写。
 * 背景：实机空 TRACE 时无法分层；本树先保证 SIM 上标记可达。
 */
__aicore__ inline void CleanTraceMark(GM_ADDR ws, tiling::CleanTraceStage stage, bool aic, int32_t subBlockID)
{
    if (!aic && subBlockID != 0) {
        return;
    }
    auto *t = reinterpret_cast<__gm__ int32_t *>(ws + tiling::TRACE);
    t[static_cast<int32_t>(stage)] = 1;
}

/** AIC：一轮 WAIT(1) → MMAD → SET(3)。 */
__aicore__ inline void AicOneCubeRound(GM_ADDR ws)
{
    FsmState st = ST_AIV_SPLIT;
    FsmWait(st);
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
 * AIV：填 A → SET(1) → WAIT(3)；可选 light pack。
 * @param tag 轮次偏移
 * @param doLightPack 为 true 时 AIV0 读 MAT_C 首段到 STUB
 */
__aicore__ inline void AivOneCubeRound(int32_t subBlockID, GM_ADDR ws, int8_t tag, bool doLightPack)
{
    FillLeftMatrixA(subBlockID, ws, tag);
    FsmState st = ST_AIV_SPLIT;
    FsmSet(st);
    st = ST_AIV_PACK;
    FsmWait(st);
    if (doLightPack && subBlockID == 0) {
        AscendC::TPipe pipe;
        AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ;
        pipe.InitBuffer(inQ, 1, 8 * sizeof(int32_t));
        AscendC::LocalTensor<int32_t> ub = inQ.AllocTensor<int32_t>();
        AscendC::GlobalTensor<int32_t> cGm;
        cGm.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::MAT_C), tiling::kRows * tiling::kCols);
        AscendC::DataCopy(ub, cGm, 8);
        inQ.EnQue(ub);
        ub = inQ.DeQue<int32_t>();
        AscendC::GlobalTensor<int32_t> stubGm;
        stubGm.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::STUB), tiling::kStubVecElems);
        AscendC::DataCopy(stubGm, ub, 8);
        inQ.FreeTensor(ub);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * MIX kernel：1 AIC + 2 AIV；由 tiling.phase 选 L1 / L2。
 * @param out magic 输出 64B（仅 L2 写入）
 * @param src 占位输入
 * @param ws  workspace（含 TRACE）
 * @param tiling phase=0/1
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    (void)src;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    FsmState st;
    const int32_t phase = tiling.phase;

    if (phase == kPhaseLaunch1) {
        // ========== Launch1：prep+NTT（一轮 Cube；无设备 μ、无 Wait(4)）==========
        if (aic) {
            AicOneCubeRound(ws);
        } else {
            StubHashPrep(subBlockID, ws);
            AivOneCubeRound(subBlockID, ws, /*tag=*/1, /*doLightPack=*/true);
            StubInner(subBlockID, ws);
        }
        return;
    }

    // ========== Launch2：skipNtt（结构默认：无 PrefixEmbed）+ P1a TRACE ==========
    if (aic) {
        st = ST_IP_AIV_DONE;
        FsmWait(st);
        CleanTraceMark(ws, TR_AIC_AFTER_WAIT4, aic, subBlockID);
        st = ST_AT_JP_GATE;
        FsmSet(st);
        CleanTraceMark(ws, TR_AIC_AFTER_GATE, aic, subBlockID);
        AicOneCubeRound(ws);
    } else {
        CleanTraceMark(ws, TR_AIV_BEFORE_ATJP, aic, subBlockID);
        // 短 at_jp stub — **绝不**调用 PrefixEmbed / StubPrefixEmbedMu
        StubAtJpLight(subBlockID, ws);
        CleanTraceMark(ws, TR_AIV_AFTER_ATJP, aic, subBlockID);
        // 双 AIV 均 SET(4)，保证 AIC Wait(4) 可达（第一不变量）
        st = ST_IP_AIV_DONE;
        FsmSet(st);
        CleanTraceMark(ws, TR_AIV_AFTER_SET4, aic, subBlockID);
        st = ST_AT_JP_GATE;
        FsmWait(st);
        CleanTraceMark(ws, TR_AIV_AFTER_GATE, aic, subBlockID);
        AscendC::PipeBarrier<PIPE_ALL>();
        AivOneCubeRound(subBlockID, ws, /*tag=*/20, /*doLightPack=*/false);
        StubEncodeMagic(subBlockID, out, tiling::kMagicCleanHostMu);
    }
}
