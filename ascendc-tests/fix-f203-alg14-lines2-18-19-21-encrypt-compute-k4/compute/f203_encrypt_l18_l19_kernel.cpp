/**
 * @file f203_encrypt_l18_l19_kernel.cpp
 * @brief Alg.14 行 18–19 可行性：单 MIX launch 验证 NTT(y)→SYNC→at_jp→SYNC→INTT→+e₁。
 *
 * FSM（对齐 stage123 / INTEGRATION_PLAN §4.3：CrossCore 仅 AIC↔AIV，无 AIV↔AIV）：
 *   NTT:  AIV_SPLIT → AIC_MMAD → AIV_PACK → y_hat GM
 *   IP:   内积 û 驻留 UB → INTT split(ProcessFromLocal) → DataCopy 落盘 u_ntt GM（仅对拍）
 *   GATE: AIC WAIT IP → SET AT_JP_GATE → 双 AIV WAIT GATE → SET flag1 释放 AIC INTT MMAD
 *   INTT: 与 intt_e1 相同 flag **1/3**（禁止用 flag 2；三 launch 证明 MMAD@LUT offset0 正确）
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_encrypt_at_jp_scalar.hpp"
#include "f203_l18_l19_tiling.h"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

enum FsmState : uint16_t {
    ST_NTT_AIV_SPLIT = 1,
    ST_NTT_AIC_MMAD = 2,
    ST_NTT_AIV_PACK = 3,
    ST_IP_AIV_DONE = 4,
    /** AIC 确认双 AIV 内积段结束，释放 INTT S1（qa §11.4 / INTEGRATION_PLAN §4.3） */
    ST_AT_JP_GATE = 8,
};

#ifdef ASCENDC_CPU_DEBUG
volatile int g_f203_l18_l19_mix_pass = 3;
#endif

/** 分段进度：traceGm[kStage] = 1（仅 AIV subBlock0 写，供 host 轮询判死锁）。 */
enum FusedTraceStage : int32_t {
    TR_AIV_NTT_SPLIT = 0,
    TR_AIC_NTT_MMAD = 1,
    TR_AIV_NTT_YHAT = 2,
    TR_AIV_AT_JP_START = 3,
    TR_AIV_AT_JP_DONE = 4,
    TR_AIV_IP_SIGNAL = 5,
    TR_AIC_IP_WAIT_DONE = 6,
    TR_AIV_INTT_SPLIT = 7,
    TR_AIC_INTT_MMAD = 8,
    TR_AIV_INTT_U = 9,
    TR_AIV_E1_DONE = 10,
    TR_AIC_AT_JP_GATE = 11,
    TR_AIV_AT_JP_GATE = 12,
    TR_COUNT = 16,
};

__aicore__ inline void FusedTraceMark(GM_ADDR traceGm, FusedTraceStage stage, const bool aic, int32_t subBlockID)
{
    if (traceGm == nullptr) {
        return;
    }
    if (!aic && subBlockID != 0) {
        return;
    }
    auto *trace = reinterpret_cast<__gm__ int32_t *>(traceGm);
    trace[static_cast<int32_t>(stage)] = 1;
}

__aicore__ inline void FsmWait(FsmState st, const bool aic, const int32_t subBlockID)
{
    (void)aic;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

__aicore__ inline void FsmSet(FsmState st, const bool aic, const int32_t subBlockID)
{
    (void)aic;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/** 时域 mod-q 加：u[p] ← (time[p] + e1[p]) mod q，按 poly 分片。 */
__aicore__ inline void AddE1Halfrows(GM_ADDR uOut, GM_ADDR uTime, GM_ADDR e1, int32_t subBlockID)
{
    constexpr int32_t kN = encrypt_at_jp::kN;
    constexpr int32_t kQ = encrypt_at_jp::kQ;
    const int32_t pBegin = subBlockID * 2;
    const int32_t pEnd = pBegin + 2;
    auto *uGm = reinterpret_cast<__gm__ int32_t *>(uOut);
    const auto *tGm = reinterpret_cast<const __gm__ int32_t *>(uTime);
    const auto *eGm = reinterpret_cast<const __gm__ int32_t *>(e1);
    for (int32_t p = pBegin; p < pEnd; ++p) {
        for (int32_t c = 0; c < kN; ++c) {
            const uint32_t off = static_cast<uint32_t>(p) * static_cast<uint32_t>(kN) + static_cast<uint32_t>(c);
            int32_t v = tGm[off] + eGm[off];
            v %= kQ;
            if (v < 0) {
                v += kQ;
            }
            uGm[off] = v;
        }
    }
}

__aicore__ inline void AicMmadRound(GM_ADDR ws, uint32_t coeffN, size_t lutEvenTop, size_t lutOddTop)
{
    using namespace tiling;
    AicMmad mmad(static_cast<uint16_t>(mRowsLogic), coeffN, static_cast<uint16_t>(halfN));
    mmad.Init();
    mmad.Process(ws + MAT_C_TMP_LO_EVEN, ws + S0, ws + lutEvenTop);
    KYBER_PIPE_ALL();
    mmad.Process(ws + MAT_C_TMP_LO_ODD, ws + S0, ws + lutOddTop);
    KYBER_PIPE_ALL();
    mmad.Process(ws + MAT_C_TMP_HI_EVEN, ws + S0, ws + lutEvenTop + n * lutPlanarCols);
    KYBER_PIPE_ALL();
    mmad.Process(ws + MAT_C_TMP_HI_ODD, ws + S0, ws + lutOddTop + n * lutPlanarCols);
    KYBER_PIPE_ALL();
}

extern "C" __global__ __aicore__ void f203_encrypt_l18_l19(GM_ADDR uOut, GM_ADDR ySrc, GM_ADDR yHat, GM_ADDR uNtt,
                                                           GM_ADDR aHat, GM_ADDR e1, GM_ADDR ws, TilingData tiling,
                                                           GM_ADDR traceGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    FsmState st;

    if (aic) {
        /* ── 行 18 NTT Stage2 ── */
        st = ST_NTT_AIV_SPLIT;
        FsmWait(st, aic, subBlockID);
        st = ST_NTT_AIC_MMAD;
        AicMmadRound(ws, coeffN, LUT_NTT_EVEN_TOP, LUT_NTT_ODD_TOP);
        FusedTraceMark(traceGm, TR_AIC_NTT_MMAD, aic, subBlockID);
        st = ST_NTT_AIV_PACK;
        FsmSet(st, aic, subBlockID);

        /* 内积阶段 AIC 空转，直至 AIV 完成 at_jp */
        st = ST_IP_AIV_DONE;
        FsmWait(st, aic, subBlockID);
        FusedTraceMark(traceGm, TR_AIC_IP_WAIT_DONE, aic, subBlockID);
        st = ST_AT_JP_GATE;
        FsmSet(st, aic, subBlockID);
        FusedTraceMark(traceGm, TR_AIC_AT_JP_GATE, aic, subBlockID);

        /* ── 行 19 INTT Stage2：与 intt_e1 相同 WAIT1 → MMAD → SET3 ── */
        st = ST_NTT_AIV_SPLIT;
        FsmWait(st, aic, subBlockID);
        AicMmadRound(ws, coeffN, LUT_INTT_EVEN_STACKED, LUT_INTT_ODD_STACKED);
        FusedTraceMark(traceGm, TR_AIC_INTT_MMAD, aic, subBlockID);
        st = ST_NTT_AIV_PACK;
        FsmSet(st, aic, subBlockID);
    } else {
        /* ── 行 18 NTT Stage1 ── */
        {
            st = ST_NTT_AIV_SPLIT;
            AivK8Split splitNtt(subBlockID, coeffN);
            splitNtt.Init(ws + S0, ySrc);
            splitNtt.Process();
            KYBER_PIPE_ALL();
            FsmSet(st, aic, subBlockID);
            FusedTraceMark(traceGm, TR_AIV_NTT_SPLIT, aic, subBlockID);
        }

        /* ── 行 18 NTT Pack（与 S3 merge 分作用域，各持独立 TPipe）── */
        {
            st = ST_NTT_AIV_PACK;
            FsmWait(st, aic, subBlockID);
            AivK8PackMatCPlanar packNtt(subBlockID, coeffN);
            packNtt.Init(ws + MAT_C_PLANAR, ws + MAT_C_TMP_LO_EVEN, ws + MAT_C_TMP_LO_ODD, ws + MAT_C_TMP_HI_EVEN,
                          ws + MAT_C_TMP_HI_ODD);
            packNtt.Process();
            KYBER_PIPE_ALL();
        }
        {
            AivK8RouteAMod mergeNtt(subBlockID, coeffN);
            mergeNtt.Init(yHat, ws + MAT_C_PLANAR);
            mergeNtt.Process();
            KYBER_PIPE_ALL();
        }
        FusedTraceMark(traceGm, TR_AIV_NTT_YHAT, aic, subBlockID);

        /* ★ SYNC-ŷ */
        KYBER_PIPE_ALL();

        /* ── 行 19：内积 UB 驻留 → INTT S1 split → u_ntt 落盘 ── */
        {
            FusedTraceMark(traceGm, TR_AIV_AT_JP_START, aic, subBlockID);
            const int32_t pBegin = subBlockID * 2;
            const int32_t pEnd = pBegin + 2;
            const uint32_t ubElems = tiling::kPolysPerAiv * coeffN;

            AscendC::TPipe ipPipe;
            AscendC::TQue<AscendC::TPosition::VECIN, 1> queUbU;
            ipPipe.InitBuffer(queUbU, 1, ubElems * sizeof(int32_t));
            AscendC::LocalTensor<int32_t> ubUNtt = queUbU.AllocTensor<int32_t>();

            encrypt_at_jp::innerproduct_halfrows_to_ub(aHat, yHat, ubUNtt, pBegin, pEnd);
            AscendC::PipeBarrier<PIPE_ALL>();
            FusedTraceMark(traceGm, TR_AIV_AT_JP_DONE, aic, subBlockID);

            {
                AivK8Split splitIntt(subBlockID, coeffN);
                splitIntt.Init(ws + S0, uNtt);
                splitIntt.ProcessFromLocal(ubUNtt);
                AscendC::PipeBarrier<PIPE_ALL>();
                FusedTraceMark(traceGm, TR_AIV_INTT_SPLIT, aic, subBlockID);
            }

            encrypt_at_jp::dump_u_ntt_halfrows_ub(uNtt, ubUNtt, pBegin, pEnd);
            queUbU.FreeTensor(ubUNtt);
        }

        /* 双 AIV 均 SET（同 stage123 S1）；禁止仅 subBlock0 SET 导致 AIC 提前进 INTT */
        st = ST_IP_AIV_DONE;
        FsmSet(st, aic, subBlockID);
        if (subBlockID == 0) {
            FusedTraceMark(traceGm, TR_AIV_IP_SIGNAL, aic, subBlockID);
        }

        st = ST_AT_JP_GATE;
        FsmWait(st, aic, subBlockID);
        FusedTraceMark(traceGm, TR_AIV_AT_JP_GATE, aic, subBlockID);
        KYBER_PIPE_ALL();

        /* S0 已含 INTT Stage1；释放 AIC MMAD（flag 1），完成后 AIC SET 3 */
        st = ST_NTT_AIV_SPLIT;
        FsmSet(st, aic, subBlockID);

        {
            st = ST_NTT_AIV_PACK;
            FsmWait(st, aic, subBlockID);
            AivK8PackMatCPlanar packIntt(subBlockID, coeffN);
            packIntt.Init(ws + MAT_C_PLANAR, ws + MAT_C_TMP_LO_EVEN, ws + MAT_C_TMP_LO_ODD, ws + MAT_C_TMP_HI_EVEN,
                          ws + MAT_C_TMP_HI_ODD);
            packIntt.Process();
            KYBER_PIPE_ALL();
        }
        {
            AivK8RouteAMod mergeIntt(subBlockID, coeffN);
            mergeIntt.Init(uOut, ws + MAT_C_PLANAR);
            mergeIntt.Process();
            KYBER_PIPE_ALL();
        }
        FusedTraceMark(traceGm, TR_AIV_INTT_U, aic, subBlockID);

        AscendC::PipeBarrier<PIPE_ALL>();
        AddE1Halfrows(uOut, uOut, e1, subBlockID);
        KYBER_PIPE_ALL();
        FusedTraceMark(traceGm, TR_AIV_E1_DONE, aic, subBlockID);
    }
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_encrypt_l18_l19_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uOut, uint8_t *ySrc,
                                        uint8_t *yHat, uint8_t *uNtt, uint8_t *aHat, uint8_t *e1, uint8_t *ws,
                                        uint8_t *tiling, uint8_t *traceGm)
{
    f203_encrypt_l18_l19<<<blockDim, l2ctrl, stream>>>(uOut, ySrc, yHat, uNtt, aHat, e1, ws,
                                                       reinterpret_cast<TilingData *>(tiling), traceGm);
}
#endif
