/**
 * @file mmad_custom.cpp
 * @brief ER01：Encrypt 外形双 launch 设备核（phase=PREP / COMPUTE）。
 *
 * 图谱：D-EXP-ER01 → enc_related 首刀。
 *
 * Host 串行：
 *   Launch1 phase=PREP：双 AIV 轻量 mixing → SAMPLE_OUT；AIC 立即返回（无 CrossCore）
 *   Launch2 phase=COMPUTE：NTT 1/3 + 生产 GATE 4/8（真 Vec MAC）+ INTT 复用 1/3
 *
 * COMPUTE 握手（禁 SyncAll@AIC-Wait、禁 SoftSync、禁 INTT flag 5/7、禁 X14 空转）：
 *   NTT：双 AIV SET(1) → AIC WAIT(1)+极轻 Cube → AIC SET(3) → 双 AIV WAIT(3)
 *   GATE（生产时序）：
 *     AIC：SET(3) 后 TRACE(403) → WAIT(4)（先占坑）
 *     AIV：WAIT(3) 后真 Vec MAC → TRACE(204/304) → 双 AIV SET(4)
 *     AIC：WAIT(4) 返回 → TRACE(404) → SET(8)
 *     AIV：WAIT(8) → TRACE(205/305)
 *   INTT（复用 flag 1/3，非 5/7）：
 *     AIC：SET(8) 后 → WAIT(1)+极轻 Cube → SET(3)
 *     AIV：WAIT(8) 后 → 桩写 S0 → SET(1) → WAIT(3) → 完成标记
 *
 * AscendC API：CrossCoreSetFlag/WaitFlag、DataCopy、Duplicate、Mul/Add/Muls、Mmad 等
 * 复用查阅索引既有记录（GT-20260903-* / T06）；本刀无新增 API。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

/**
 * CrossCore FSM：NTT/INTT 用 1/3；GATE 用 4/8。
 * **禁止** 使用 5/7（KB X1）。
 */
enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1,
    ST_AIC_MMAD = 2,
    ST_AIV_PACK = 3,
    ST_GATE_AIV = 4,
    ST_GATE_AIC = 8,
};

/**
 * 等待对端 CrossCore 置位；通道 <2, PIPE_MTE2>。
 * 背景：AIC 在 Wait 期间 **禁止** SyncAll（KB X2）。
 */
__aicore__ inline void FsmWait(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/** 向对端广播 FSM 完成。 */
__aicore__ inline void FsmSet(FsmState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * TRACE 写槽：LocalTensor 赋值后 DataCopy 到 GM；禁止 `__gm__ int32` 直写（KB X9）。
 * @param traceGm TRACE GM 基址（可空）
 * @param ws      workspace（AIC 读 TRACE_ONES）
 * @param slot    逻辑槽
 * @param aic     true=AIC（走 A1+ones 模板）
 */
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

/** NTT/INTT 段极轻 Cube：C[16,32]=S0[16,32]@LUT[32,32]。 */
__aicore__ inline void RunLightCubeMmad(GM_ADDR ws)
{
    AicMmad mmad(static_cast<uint16_t>(tiling::kRows), static_cast<uint16_t>(tiling::kDim),
                 static_cast<uint16_t>(tiling::kCols));
    mmad.Init();
    mmad.Process(ws + tiling::MAT_C, ws + tiling::S0, ws + tiling::LUT);
    KYBER_PIPE_ALL();
}

/**
 * Launch1 prep：仅 AIV 做采样桩；AIC 立即返回（无 CrossCore，避免空等）。
 * 结论：prep 与计算壳分 launch，外形贴近 Encrypt；桩体量有界、无 SHAKE。
 */
__aicore__ inline void RunPrepPhase(GM_ADDR /*out*/, GM_ADDR ws, GM_ADDR trace, const bool aic,
                                    int32_t subBlockID)
{
    if (aic) {
        return;
    }
    ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_SAMPLE_START : TR_AIV1_SAMPLE_START, aic,
                 subBlockID);
    {
        AivSampleStub sample(subBlockID);
        sample.Init(ws);
        sample.Process();
        KYBER_PIPE_ALL();
    }
    ToyTraceMark(trace, ws, subBlockID == 0 ? TR_AIV0_SAMPLE_DONE : TR_AIV1_SAMPLE_DONE, aic,
                 subBlockID);
}

/**
 * Launch2 计算壳：AIC / AIV 按生产 GATE 时序跑完整 NTT→GATE→INTT。
 * 未采用项：INTT 换 5/7；对称 GATE；假循环体量。
 */
__aicore__ inline void RunComputePhase(GM_ADDR out, GM_ADDR ws, GM_ADDR trace, const bool aic,
                                       int32_t subBlockID)
{
    FsmState st;

    if (aic) {
        st = ST_AIV_SPLIT;
        FsmWait(st);
        ToyTraceMark(trace, ws, TR_AIC_WAIT1, aic, subBlockID);

        RunLightCubeMmad(ws);

        ToyTraceMark(trace, ws, TR_AIC_SET3, aic, subBlockID);
        st = ST_AIV_PACK;
        FsmSet(st);

        /* 生产 GATE：NTT 后 AIC 立刻 WAIT(4) 占坑 */
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
        /* NTT：SAMPLE_OUT → S0 → SET(1) */
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

        /* GATE：真积木 Vec MAC 后再双 AIV SET(4) */
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

/**
 * 入口：按 tiling.phase 分派 PREP / COMPUTE。
 * @param out   完成标记 GM（仅 COMPUTE 写）
 * @param ws    workspace
 * @param trace TRACE GM
 * @param tiling phase∈{0,1}
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR ws, GM_ADDR trace,
                                                    TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const int32_t phase = tiling.phase;

    if (phase == ER01_PHASE_PREP) {
        RunPrepPhase(out, ws, trace, aic, subBlockID);
        return;
    }
    RunComputePhase(out, ws, trace, aic, subBlockID);
}
