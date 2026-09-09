/**
 * @file mmad_custom.cpp
 * @brief E20 stub：全 Mark/CrossCore 之后 AIC 早退 + 双 AIV 非对称尾包 DataCopy。
 *
 * 同步序（E17 壳；对照 ENCRYPT_GAP / l18 FSM，禁抄 Encrypt / tail_pack 实现）：
 *   伪 NTT：AIV SET(1) ↔ AIC Wait(1)；AIC SET(3) ↔ AIV Wait(3)
 *   GATE：  双 AIV SET(4) ↔ AIC Wait(4)；AIC SET(8) ↔ 双 AIV Wait(8)
 *   伪 INTT：再次 AIV SET(1) ↔ AIC Wait(1)；AIC SET(3) ↔ AIV Wait(3)  ← 复用 1/3
 *   **之后无 CrossCore**：AIC 在 INTT 末次 FsmSet(3) 后立即 return；
 *   双 AIV 打 postmark TRACE，再各自跑若干轮自建 TPipe+DataCopy GM↔UB（假数据）。
 *
 * 背景：图谱 J-hang-after-full-trace — 真挂窗偏全 Mark 之后；SIM 不能复现粘性，
 * 本刀只证「该结构在 SIM 可活 + sync_audit 无真死等」。
 * 结论：尾包仅为 stub 负载非对称，禁止 #include encrypt/tail_pack。
 * 未采用：真 mod_q / ByteEncode / f203_tail_pack_ops；OMIT 对照。
 *
 * CrossCore：modeId=2、PIPE_MTE2；flag ∈ {1,3,4,8}；Wait 路径禁止 SyncAll。
 * API：复用仓内已查 CrossCore Set/Wait、PipeBarrier、DataCopy、TPipe、TQue（见查阅索引）。
 */
#include "kernel_operator.h"
#include "tiling.h"

/** CrossCore flag：对齐 l18 FSM 号（stub 语义，非业务状态机实现）。 */
enum FsmState : uint16_t {
    ST_NTT_AIV_SPLIT = 1, /**< AIV→AIC：伪 Stage1 完成 / 释放伪 MMAD */
    ST_NTT_AIV_PACK = 3,  /**< AIC→AIV：伪 Stage2 完成 / 放行 Pack */
    ST_IP_AIV_DONE = 4,   /**< 双 AIV→AIC：伪内积齐（GATE 入） */
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
 * @param st flagId（1/3/4/8）
 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * 置位 CrossCore。
 * @param st flagId（1/3/4/8）
 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * AIV0 写极简 magic（证明「全 Mark 之后」尾包仍跑完）；AIV1 / AIC 不写。
 * UB 填常量后 DataCopy → GM（对齐 32B；64B=2 block）。
 * @param out GM 输出 64B
 */
__aicore__ inline void WriteMagic(GM_ADDR out)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    pipe.InitBuffer(outQ, 1, tiling::kOutBytes);
    AscendC::LocalTensor<uint8_t> ub = outQ.AllocTensor<uint8_t>();
    // 写 8B 前缀 "E20TOY01"
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
 * 全 CrossCore 结束后的 AIV-only 尾包 stub：若干轮 GM↔UB DataCopy（假数据）。
 *
 * 背景：真 l18 上 AIC 在 INTT FsmSet(PACK) 后即结束；AIV 仍做 mod_q + tail_pack，
 * 之后无 CrossCore。本函数仅用自建 TPipe+DataCopy 模拟非对称负载，禁抄业务。
 *
 * @param ws       全局 workspace；本 AIV 使用 [subBlockID * stride, …)
 * @param subBlockID 0=多干（heavy rounds），1=少干（light rounds）
 * @param rounds   本 AIV 尾包轮数
 */
__aicore__ inline void PostmarkTailStub(GM_ADDR ws, int32_t subBlockID, uint32_t rounds)
{
    // postmark：设备 TRACE（Host 侧见 100/111；本号标「已过全 Mark」）
    if (subBlockID == 0) {
        TraceDigit(540); // AIV0 postmark / 将进尾包
    } else {
        TraceDigit(550); // AIV1 postmark / 将进尾包
    }

    const size_t base = static_cast<size_t>(subBlockID) * tiling::kAivWsStride;

    // 每轮：UB→GM 假写 + GM→UB 假读；轮间独立 TPipe（对齐 E19 RMW 风格，禁业务）
    for (uint32_t r = 0; r < rounds; ++r) {
        AscendC::GlobalTensor<uint8_t> chunkGm;
        chunkGm.SetGlobalBuffer((__gm__ uint8_t *)ws + base + r * tiling::kStubChunkBytes,
                                tiling::kStubChunkBytes);

        // ---- UB → GM（假写）----
        {
            AscendC::TPipe pipe;
            AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
            pipe.InitBuffer(outQ, 1, tiling::kStubChunkBytes);
            AscendC::LocalTensor<uint8_t> ub = outQ.AllocTensor<uint8_t>();
            // 仅写前缀标记；其余保持 Alloc 初值即可（stub 不关心内容）
            ub.SetValue(0, static_cast<uint8_t>(0x20 + subBlockID));
            ub.SetValue(1, static_cast<uint8_t>(r & 0xFF));
            outQ.EnQue(ub);
            ub = outQ.DeQue<uint8_t>();
            AscendC::DataCopy(chunkGm, ub, tiling::kStubChunkBytes);
            outQ.FreeTensor(ub);
            AscendC::PipeBarrier<PIPE_ALL>();
        }

        // ---- GM → UB（假读）----
        {
            AscendC::TPipe pipe;
            AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ;
            pipe.InitBuffer(inQ, 1, tiling::kStubChunkBytes);
            AscendC::LocalTensor<uint8_t> ub = inQ.AllocTensor<uint8_t>();
            AscendC::DataCopy(ub, chunkGm, tiling::kStubChunkBytes);
            inQ.EnQue(ub);
            ub = inQ.DeQue<uint8_t>();
            // 读回后丢弃（证明搬运通路走通；不写 Host）
            (void)ub.GetValue(0);
            inQ.FreeTensor(ub);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
    }

    if (subBlockID == 0) {
        TraceDigit(541); // AIV0 尾包完成
    } else {
        TraceDigit(551); // AIV1 尾包完成
    }
}

/**
 * MIX kernel：1 AIC + 2 AIV；单 launch = E17 全序 + AIC 早退 + AIV 尾包。
 * @param out [out] AIV0 写 magic（尾包之后）
 * @param src [in]  占位（未用）
 * @param ws  [in/out] 假数据区，供 PostmarkTailStub 读写
 * @param tiling 占位（本 toy 无 phase）
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    (void)src;
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

        // ========== 伪 INTT：复用 Wait(1)/Set(3) ==========
        TraceDigit(420); // 将 Wait(1) INTT 复用
        FsmWait(ST_NTT_AIV_SPLIT);
        TraceDigit(421); // Wait(1) 返回；stub 跳过 INTT MMAD
        FsmSet(ST_NTT_AIV_PACK);
        TraceDigit(423); // 已 Set(3)；模拟真 l18：AIC 立即早退，无后续 CrossCore
        return;
    } else {
        // ========== 伪 NTT：双 AIV Set(1) → Wait(3) ==========
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

        // ========== 伪 INTT：复用 Set(1) → Wait(3) ==========
        FsmSet(ST_NTT_AIV_SPLIT);
        if (subBlockID == 0) {
            TraceDigit(521);
        } else {
            TraceDigit(531);
        }
        FsmWait(ST_NTT_AIV_PACK);
        if (subBlockID == 0) {
            TraceDigit(523);
        } else {
            TraceDigit(533);
        }

        // ========== postmark 尾包：此后无 CrossCore；AIC 已早退 ==========
        const uint32_t tailRounds =
            (subBlockID == 0) ? tiling::kStubRoundsHeavy : tiling::kStubRoundsLight;
        PostmarkTailStub(ws, subBlockID, tailRounds);

        // 仅 AIV0 写 magic，证明全序 + 尾包可达
        if (subBlockID == 0) {
            WriteMagic(out);
        }
    }
}
