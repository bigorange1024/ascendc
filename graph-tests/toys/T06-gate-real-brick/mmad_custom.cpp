/**
 * @file mmad_custom.cpp
 * @brief T06：NTT 1/3 + 生产 GATE 4/8（AIV 真积木 Vec MAC）+ INTT 复用 1/3（单 launch 全 FSM）。
 *
 * 图谱：D-EXP-T06 → Q-REPRO-ON-SIM / F-BRICKS-OK / F-BAN-INTT-57。
 *
 * 握手顺序（禁 SyncAll@AIC-Wait、禁 SoftSync、**禁 INTT flag 5/7**，KB X1）：
 *   NTT：双 AIV SET(1) → AIC WAIT(1)+极轻 Cube → AIC SET(3) → 双 AIV WAIT(3)
 *   GATE（生产时序，同 T02/T03）：
 *     AIC：SET(3) 后 **立刻** TRACE(403) → WAIT(4)
 *     AIV：WAIT(3) 后 **真 Vec MAC**（int32[64]×8 轮 Mul+Add）→ TRACE(204/304) → 双 AIV SET(4)
 *     AIC：WAIT(4) 返回 → TRACE(404) → SET(8)
 *     AIV：WAIT(8) → TRACE(205/305)
 *   INTT（第二轮，**复用 flag 1/3**，非 5/7）：
 *     AIC：SET(8) 后 → WAIT(1)+极轻 Cube → SET(3)
 *     AIV：WAIT(8) 后 → 桩写 S0 → SET(1) → WAIT(3) → 完成标记
 *
 * 禁 X14：GATE 段不得用标量 SetValue 假循环冒充体量。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1,
    ST_AIC_MMAD = 2,
    ST_AIV_PACK = 3,
    ST_GATE_AIV = 4,
    ST_GATE_AIC = 8,
};

__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

__aicore__ inline void ToyTraceMark(GM_ADDR traceGm, GM_ADDR ws, ToyTraceSlot slot, const bool aic,
                                    int32_t /*subBlockID*/)
{
    if (traceGm == nullptr) {
        return;
    }

    constexpr uint32_t kAlign = static_cast<uint32_t>(tiling::kTraceAlignInts);
    const uint32_t slotOffInts = static_cast<uint32_t>(slot) * kAlign;

    AscendC::GlobalTensor<int32_t> dstGm;
    dstGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(traceGm) + slotOffInts, kAlign);

    AscendC::TPipe pipe;
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

__aicore__ inline void RunLightCubeMmad(GM_ADDR ws)
{
    AicMmad mmad(static_cast<uint16_t>(tiling::kRows), static_cast<uint16_t>(tiling::kDim),
                 static_cast<uint16_t>(tiling::kCols));
    mmad.Init();
    mmad.Process(ws + tiling::MAT_C, ws + tiling::S0, ws + tiling::LUT);
    KYBER_PIPE_ALL();
}

extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR ws, GM_ADDR trace,
                                                    TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    FsmState st;

    if (aic) {
        st = ST_AIV_SPLIT;
        FsmWait(st);
        ToyTraceMark(trace, ws, TR_AIC_WAIT1, aic, subBlockID);

        RunLightCubeMmad(ws);

        ToyTraceMark(trace, ws, TR_AIC_SET3, aic, subBlockID);
        st = ST_AIV_PACK;
        FsmSet(st);

        ToyTraceMark(trace, ws, TR_AIC_WAIT4, aic, subBlockID);
        st = ST_GATE_AIV;
        FsmWait(st);

        ToyTraceMark(trace, ws, TR_AIC_SET8, aic, subBlockID);
        st = ST_GATE_AIC;
        FsmSet(st);

        st = ST_AIV_SPLIT;
        FsmWait(st);
        ToyTraceMark(trace, ws, TR_AIC_INTT_WAIT1, aic, subBlockID);

        RunLightCubeMmad(ws);

        ToyTraceMark(trace, ws, TR_AIC_INTT_SET3, aic, subBlockID);
        st = ST_AIV_PACK;
        FsmSet(st);
    } else {
        {
            AivStubHashSplit split(subBlockID);
            split.Init(ws);
            split.Process();
            KYBER_PIPE_ALL();
        }
        {
            st = ST_AIV_SPLIT;
            ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_SET1 : TR_AIV1_SET1, aic, subBlockID);
            FsmSet(st);
        }

        st = ST_AIV_PACK;
        FsmWait(st);
        ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_WAIT3 : TR_AIV1_WAIT3, aic, subBlockID);

        /* GATE：真积木 Vec MAC（禁 X14 假循环） */
        {
            AivGateRealBrickMac brick(subBlockID);
            brick.Init(ws);
            brick.Process();
            KYBER_PIPE_ALL();
        }
        {
            st = ST_GATE_AIV;
            ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_SET4 : TR_AIV1_SET4, aic, subBlockID);
            FsmSet(st);
        }

        st = ST_GATE_AIC;
        FsmWait(st);
        ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_WAIT8 : TR_AIV1_WAIT8, aic, subBlockID);

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
            FsmSet(st);
        }

        st = ST_AIV_PACK;
        FsmWait(st);
        ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_INTT_WAIT3 : TR_AIV1_INTT_WAIT3, aic,
                     subBlockID);

        {
            AivDoneMark mark(subBlockID);
            mark.Init(out);
            mark.Process();
            KYBER_PIPE_ALL();
        }
    }
}
