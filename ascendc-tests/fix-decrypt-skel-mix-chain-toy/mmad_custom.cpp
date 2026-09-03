/**
 * @file mmad_custom.cpp
 * @brief Decrypt fused 握手骨架 toy：单 MIX launch 串起 SoftSync + 两轮 GATE + stub Cube。
 *
 * 流水线位置：对齐生产 `f203_decrypt_device_fused_entry.cpp` 的同步顺序，
 * **不**移植 unpack / su_dot / NTT Stage1–3；只验证握手能绿 / 缺哨兵或缺 SET(4) 能挂。
 * 与 golden 无关（verify 只查 magic）。
 *
 * 握手（默认 SKEL_OMIT_SET4=0 且 SKEL_OMIT_SLOT0=0）：
 *   AIV0 stub_prep → SoftSyncArrive(slot0)；AIV1 自旋
 *   双 AIV：SET(4)→WAIT(8)→Clear(slot0)
 *   NTT-like：SET(1) / AIC WAIT(1)+MMAD+SET(3) / AIV WAIT(3)（无 flag 2）
 *   AIV0 stub_dot → SoftSyncArrive(slot1)；AIV1 自旋
 *   双 AIV：SET(4)→WAIT(8)→Clear(slot1)
 *   INTT-like：同构 flag 1/3（无 flag 2）
 *   AIV0 写 magic SKELDEC1
 *   AIC：入口 WAIT(4)→SET(8) → Cube → WAIT(4)→SET(8) → Cube
 *
 * 故障注入（互斥，run.sh 禁止同时为 1）：
 *   SKEL_OMIT_SET4=1：两轮都不 SET(4) → AIC 入口 Wait(4) 死等 → 预期 SIM 124。
 *   SKEL_OMIT_SLOT0=1：AIV0 不写 s[0]=1（Arrive 空操作）；AIV1 仍 while(s[0]==0)；
 *     SoftSync 是后续 SET(4) 的前置 → AIV1 无法齐步 SET(4) → 预期 SIM 124。
 *     AIV0 仍可走 AivGateRound 的 SET(4)（本开关不打开 OMIT_SET4）。
 *
 * 禁止：AIC Wait 期间 SyncAll；自造双向 SoftSync；真算法；滥增 Host launch。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

#ifndef SKEL_OMIT_SET4
#define SKEL_OMIT_SET4 0
#endif
#ifndef SKEL_OMIT_SLOT0
#define SKEL_OMIT_SLOT0 0
#endif

/**
 * CrossCore 事件（与生产 Decrypt fused 同编号语义）：
 *   flag 1 / 3 — NTT/INTT-like Cube；本 toy **两轮都不用 flag 2**
 *   flag 4 / 8 — GATE：AIV→AIC / AIC→AIV
 */
enum FsmState : uint16_t {
    ST_SPLIT = 1,    /**< AIV 填完左矩阵 → AIC */
    ST_MMAD = 2,     /**< 保留枚举位；本 toy 不发不收 */
    ST_PACK = 3,     /**< AIC MMAD 完成 → AIV */
    ST_AIV_DONE = 4, /**< 段末 AIV 齐步 → AIC GATE */
    ST_GATE = 8,     /**< AIC 放行 → 下一段 AIV */
};

/** 等待对端 CrossCore；PIPE_MTE2。禁止在 Wait 期间 SyncAll。 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/** 置位 CrossCore，通知对端。 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * SoftSyncArrive：生产单向定式 — AIV0 写哨兵 1，AIV1 自旋等非 0。
 * @param softSyncGm int32[2]；slot0=prep，slot1=dot
 * @param slot 0 或 1
 * @param subBlockID AIV 编号
 * 背景：F-softsync-two-slots；禁止双向汇合 / SyncAll 替代。
 *
 * SoftSync 是后续双 AIV SET(4)（AivGateRound）的前置：AIV1 必须先被放行才能参与 GATE。
 * SKEL_OMIT_SLOT0=1 且 slot==0：AIV0 故意不写 s[0]（Arrive 空操作），测「忘写哨兵」；
 * AIV1 仍 while(s[0]==0) 永久转；AIV0 仍可随后 SET(4)（勿与 OMIT_SET4 叠开）。
 */
__aicore__ inline void SoftSyncArrive(GM_ADDR softSyncGm, int32_t slot, int32_t subBlockID)
{
    auto *s = reinterpret_cast<__gm__ int32_t *>(softSyncGm);
    if (subBlockID == 0) {
#if SKEL_OMIT_SLOT0
        // 故障注入：仅 slot0 对 AIV0 空操作（不写 s[0]=1）；slot1 仍正常写哨兵
        if (slot == 0) {
            AscendC::PipeBarrier<PIPE_ALL>();
            return;
        }
#endif
        s[slot] = 1;
        AscendC::PipeBarrier<PIPE_ALL>();
    } else {
        // AIV1：始终自旋等哨兵；OMIT_SLOT0 时 s[0] 永 0 → 永久转（假说成功证据）
        while (s[slot] == 0) {
        }
        AscendC::PipeBarrier<PIPE_ALL>();
    }
}

/**
 * SoftSyncClear：仅 AIV0 清哨兵，供下一段复用同一 slot。
 * @param softSyncGm / slot / subBlockID 同 Arrive
 */
__aicore__ inline void SoftSyncClear(GM_ADDR softSyncGm, int32_t slot, int32_t subBlockID)
{
    if (subBlockID == 0) {
        reinterpret_cast<__gm__ int32_t *>(softSyncGm)[slot] = 0;
        AscendC::PipeBarrier<PIPE_ALL>();
    }
}

/**
 * AIC：一轮「WAIT(1) → 轻量 MMAD → SET(3)」Cube 握手（无 flag 2）。
 * @param ws workspace（S0 / LUT / MAT_C）
 */
__aicore__ inline void AicOneCubeRound(GM_ADDR ws)
{
    FsmState st = ST_SPLIT;
    FsmWait(st);
    {
        AicMmad mmad(static_cast<uint16_t>(tiling::kRows), static_cast<uint16_t>(tiling::kDim),
                     static_cast<uint16_t>(tiling::kCols));
        mmad.Init();
        mmad.Process(ws + tiling::MAT_C, ws + tiling::S0, ws + tiling::LUT);
        KYBER_PIPE_ALL();
    }
    st = ST_PACK;
    FsmSet(st);
}

/**
 * AIV：一轮「填 A → SET(1) → WAIT(3)」；可选轻量读 MAT_C（仅首轮）。
 * @param tag 写入左矩阵的轮次偏移
 * @param doLightPack 为 true 时 AIV0 读 MAT_C 首段到 STUB（非真 RouteA）
 */
__aicore__ inline void AivOneCubeRound(int32_t subBlockID, GM_ADDR ws, int8_t tag, bool doLightPack)
{
    FillLeftMatrixA(subBlockID, ws, tag);
    // SET(1)：通知 AIC 左矩阵已就绪
    FsmState st = ST_SPLIT;
    FsmSet(st);
    // WAIT(3)：等 AIC MMAD 完成
    st = ST_PACK;
    FsmWait(st);
    if (doLightPack && subBlockID == 0) {
        AscendC::TPipe pipe;
        AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ;
        // 32B 对齐：至少搬 8 个 int32
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
 * 段末 GATE：双 AIV SET(4) → WAIT(8) → Clear(slot)；OMIT 则跳过 SET(4)。
 * @param softSyncGm 哨兵缓冲
 * @param slot 要 Clear 的 SoftSync 槽
 */
__aicore__ inline void AivGateRound(GM_ADDR softSyncGm, int32_t slot, int32_t subBlockID)
{
    FsmState st;
#if !SKEL_OMIT_SET4
    // 正常：双 AIV 均 SET(4)，与生产同构
    st = ST_AIV_DONE;
    FsmSet(st);
#else
    // 故障注入：故意不 SET(4) → AIC Wait(4) 死等（预期 SIM timeout 124）
    AscendC::PipeBarrier<PIPE_ALL>();
#endif
    // WAIT(8)：等 AIC 放行（OMIT 时 AIC 已挂，本侧可能一并卡住）
    st = ST_GATE;
    FsmWait(st);
    SoftSyncClear(softSyncGm, slot, subBlockID);
    KYBER_PIPE_ALL();
}

/**
 * MIX kernel 入口：1 AIC + 2 AIV，单趟完成 Decrypt fused 形态握手骨架。
 * @param out        [out] magic 输出，长度 64B
 * @param src        [in]  占位输入（stub 不依赖内容）
 * @param ws         [in/out] workspace：S0 / LUT / MAT_C / STUB
 * @param softSyncGm [in/out] int32[2] SoftSync 哨兵；Host 须已清零
 * @param tiling     Host 参数（本核未用字段，保留壳对齐）
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, GM_ADDR softSyncGm,
                                                  TilingData tiling)
{
    (void)src;
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    FsmState st;

    if (aic) {
        /* ========== AIC：入口 GATE → NTT Cube → GATE → INTT Cube ========== */
        // 入口 WAIT(4)：等 prep 段双 AIV SET(4)；禁止 Wait 期间 SyncAll
        st = ST_AIV_DONE;
        FsmWait(st);
        // SET(8)：放行 AIV 开 NTT-like
        st = ST_GATE;
        FsmSet(st);

        // NTT-like：WAIT(1) → MMAD → SET(3)（无 flag 2）
        AicOneCubeRound(ws);

        // 第二轮 GATE：等 su_dot 段 SET(4)
        st = ST_AIV_DONE;
        FsmWait(st);
        st = ST_GATE;
        FsmSet(st);

        // INTT-like：再一轮 Cube（仍无 flag 2）
        AicOneCubeRound(ws);
    } else {
        /* ========== AIV：prep SoftSync → GATE → NTT → dot SoftSync → GATE → INTT → magic ========== */

        // --- stub_prep（仅形态；AIV0 写 STUB）---
        StubPrep(subBlockID, ws);
        // SoftSync slot0：默认 AIV0 写 1、AIV1 自旋；OMIT_SLOT0 时 AIV0 空操作（SET(4) 前置断裂）
        SoftSyncArrive(softSyncGm, /*slot=*/0, subBlockID);

        // GATE #1 + Clear(slot0)
        AivGateRound(softSyncGm, /*slot=*/0, subBlockID);

        // NTT-like Cube
        AivOneCubeRound(subBlockID, ws, /*tag=*/0, /*doLightPack=*/true);

        // --- stub_dot（模拟 su_dot）→ SoftSync slot1 ---
        StubDot(subBlockID, ws);
        SoftSyncArrive(softSyncGm, /*slot=*/1, subBlockID);

        // GATE #2 + Clear(slot1)
        AivGateRound(softSyncGm, /*slot=*/1, subBlockID);

        // INTT-like Cube（无 flag 2）
        AivOneCubeRound(subBlockID, ws, /*tag=*/10, /*doLightPack=*/false);

        // AIV0 写 magic（合法档 out[8]=0x04）
        StubEncodeMagic(subBlockID, out, tiling::kMagicOkMark);
    }
}
