/**
 * @file mmad_custom.cpp
 * @brief T04：NTT 1/3 + 生产 GATE 4/8（AIV 加重体量）+ INTT 复用 1/3（单 launch 全 FSM）。
 *
 * 图谱：D-EXP-T04 → Q-REPRO-ON-SIM / Q-HANG-LOCUS / F-BAN-INTT-57。
 *
 * 握手顺序（禁 SyncAll@AIC-Wait、禁 SoftSync、**禁 INTT flag 5/7**，KB X1）：
 *   NTT：双 AIV SET(1) → AIC WAIT(1)+极轻 Cube → AIC SET(3) → 双 AIV WAIT(3)
 *   GATE（生产时序，同 T02/T03）：
 *     AIC：SET(3) 后 **立刻** TRACE(403) → WAIT(4)
 *     AIV：WAIT(3) 后 **40 轮**体量（T03 为 4 轮）→ TRACE(204/304) → 双 AIV SET(4)
 *     AIC：WAIT(4) 返回 → TRACE(404) → SET(8)
 *     AIV：WAIT(8) → TRACE(205/305)
 *   INTT（第二轮，**复用 flag 1/3**，非 5/7）：
 *     AIC：SET(8) 后 → WAIT(1)+极轻 Cube → SET(3)
 *     AIV：WAIT(8) 后 → 桩写 S0 → SET(1) → WAIT(3) → 完成标记
 *
 * TRACE：GT-4 式 DataCopy 写槽；Host 按 trace_map.md 打印 KB 编号（三段可区分）。
 * AscendC API：CrossCoreSetFlag/WaitFlag、DataCopy、Duplicate、Mmad 等复用 T01–T03 查阅记录。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

/**
 * CrossCore FSM：NTT/INTT 用 1/3；GATE 用 4/8。
 * **禁止** ST_INTT_* 使用 5/7（KB X1，已知 SIM 挂死路线）。
 */
enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1,  /**< NTT/INTT：AIV→AIC */
    ST_AIC_MMAD = 2,   /**< 保留编号，本玩具不单独 Wait */
    ST_AIV_PACK = 3,   /**< NTT/INTT：AIC→AIV */
    ST_GATE_AIV = 4,   /**< GATE：AIV→AIC（体量后双 SET） */
    ST_GATE_AIC = 8,   /**< GATE：AIC→AIV（WAIT4 返回后 SET） */
};

/**
 * 等待对端 CrossCore 置位；通道 <2, PIPE_MTE2> 与 F-CC-CHANNEL 一致。
 * 背景：AIC 在 Wait 期间 **禁止** SyncAll（KB X2）。
 * @param st 期望 flag（仅允许 1/3/4/8）
 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * 向对端广播 FSM 完成。
 * @param st 要置位的 flag（仅允许 1/3/4/8）
 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * TRACE 写槽（GT-4）：LocalTensor 赋值后 DataCopy 到 GM；禁止 `__gm__ int32` 直写。
 * 布局：逻辑槽 slot → GM 偏移 slot*8 个 int32（32B 对齐块）；块首元素置 1。
 * @param traceGm TRACE GM 基址（可空）
 * @param ws      workspace（AIC 读 TRACE_ONES）
 * @param slot    逻辑槽（见 ToyTraceSlot / trace_map.md）
 * @param aic     true=AIC
 */
__aicore__ inline void ToyTraceMark(GM_ADDR traceGm, GM_ADDR ws, ToyTraceSlot slot, const bool aic,
                                    int32_t /*subBlockID*/)
{
    if (traceGm == nullptr) {
        return;
    }

    constexpr uint32_t kAlign = static_cast<uint32_t>(tiling::kTraceAlignInts);
    const uint32_t slotOffInts = static_cast<uint32_t>(slot) * kAlign;

    AscendC::TPipe pipe;
    AscendC::GlobalTensor<int32_t> dstGm;
    dstGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(traceGm) + slotOffInts, kAlign);

    if (aic) {
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
 * AIC 侧极轻 Cube MMAD（NTT 与 INTT 共用同一形状，不对正确性）。
 * 背景：INTT 须复用 1/3 握手，禁止改用 5/7 专用 flag（KB X1）。
 */
__aicore__ inline void RunLightCubeMmad(GM_ADDR ws)
{
    AicMmad mmad(static_cast<uint16_t>(tiling::kRows), static_cast<uint16_t>(tiling::kDim),
                 static_cast<uint16_t>(tiling::kCols));
    mmad.Init();
    mmad.Process(ws + tiling::MAT_C, ws + tiling::S0, ws + tiling::LUT);
    KYBER_PIPE_ALL();
}

/**
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2，单 block。
 * @param out   [out] 完成标记（每 AIV 32B）
 * @param ws    [in/out] S0/LUT/MAT_C/WORK/TRACE_ONES
 * @param trace [out] TRACE 槽块；Host 预清零
 * @param tiling 占位
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR ws, GM_ADDR trace,
                                                    TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    FsmState st;

    if (aic) {
        /* ========== NTT 段：WAIT(1) → Cube → SET(3) ========== */
        st = ST_AIV_SPLIT;
        FsmWait(st);
        ToyTraceMark(trace, ws, TR_AIC_WAIT1, aic, subBlockID); // 401

        RunLightCubeMmad(ws);

        ToyTraceMark(trace, ws, TR_AIC_SET3, aic, subBlockID); // 402
        st = ST_AIV_PACK;
        FsmSet(st); // 解除双 AIV WAIT(3)；AIC **不**等 AIV WAIT(3) 完成

        /* ========== GATE 段（生产时序）：SET(3) 后立刻 WAIT(4) ========== */
        ToyTraceMark(trace, ws, TR_AIC_WAIT4, aic, subBlockID); // 403
        st = ST_GATE_AIV;
        FsmWait(st); // WAIT(4)：等双 AIV 加重体量后 SET(4)；Wait 内禁止 SyncAll

        ToyTraceMark(trace, ws, TR_AIC_SET8, aic, subBlockID); // 404
        st = ST_GATE_AIC;
        FsmSet(st); // SET(8) → 双 AIV WAIT(8)

        /* ========== INTT 段（第二轮 1/3，禁 5/7）========== */
        // SET(8) 后 AIC 立刻 WAIT(1)；AIV 可能仍在 WAIT(8)，待其 INTT SET(1) 解除
        st = ST_AIV_SPLIT;
        FsmWait(st);
        ToyTraceMark(trace, ws, TR_AIC_INTT_WAIT1, aic, subBlockID); // 405

        RunLightCubeMmad(ws);

        ToyTraceMark(trace, ws, TR_AIC_INTT_SET3, aic, subBlockID); // 406
        st = ST_AIV_PACK;
        FsmSet(st); // INTT SET(3) → 双 AIV INTT WAIT(3)
    } else {
        /* ========== NTT 段：桩写 S0 → SET(1) → WAIT(3) ========== */
        {
            AivStubHashSplit split(subBlockID);
            split.Init(ws);
            split.Process();
            KYBER_PIPE_ALL();
        }
        {
            st = ST_AIV_SPLIT;
            ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_SET1 : TR_AIV1_SET1, aic, subBlockID);
            FsmSet(st); // 201/301 后双 AIV SET(1)
        }

        st = ST_AIV_PACK;
        FsmWait(st);
        ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_WAIT3 : TR_AIV1_WAIT3, aic, subBlockID);
        // 203/303：NTT 完成

        /* ========== GATE 段：加重体量（40 轮）→ SET(4) → WAIT(8) ========== */
        {
            AivGateVolumeWorkload workload(subBlockID);
            workload.Init(ws);
            workload.Process();
            KYBER_PIPE_ALL();
        }
        {
            st = ST_GATE_AIV;
            ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_SET4 : TR_AIV1_SET4, aic, subBlockID);
            FsmSet(st); // 204/304 后双 AIV SET(4) → 解除 AIC WAIT(4)
        }

        st = ST_GATE_AIC;
        FsmWait(st);
        ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_WAIT8 : TR_AIV1_WAIT8, aic, subBlockID);
        // 205/305：GATE 完成

        /* ========== INTT 段（第二轮 1/3）：桩写 S0 → SET(1) → WAIT(3) → 完成标记 ========== */
        {
            AivStubHashSplit splitIntt(subBlockID);
            splitIntt.Init(ws);
            splitIntt.Process();
            KYBER_PIPE_ALL();
        }
        {
            st = ST_AIV_SPLIT;
            ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_INTT_SET1 : TR_AIV1_INTT_SET1, aic,
                         subBlockID);
            FsmSet(st); // 206/306：INTT SET(1) → 解除 AIC INTT WAIT(1)
        }

        st = ST_AIV_PACK;
        FsmWait(st);
        ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_INTT_WAIT3 : TR_AIV1_INTT_WAIT3, aic,
                     subBlockID);
        // 207/307：INTT 完成

        {
            AivDoneMark mark(subBlockID);
            mark.Init(out);
            mark.Process();
            KYBER_PIPE_ALL();
        }
    }
}
