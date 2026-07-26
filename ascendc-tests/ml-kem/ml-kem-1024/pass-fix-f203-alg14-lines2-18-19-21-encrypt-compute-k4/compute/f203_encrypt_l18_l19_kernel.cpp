/**
 * @file f203_encrypt_l18_l19_kernel.cpp
 * @brief Alg.14 行 2/16–17/18/19/21：**SIM 默认单 launch** 融合 MIX FSM（不含 μ）。
 *
 * 流水线位置：prep 探针产出 `a_hat`/`re` 后，本核完成 Encrypt 线性 compute 段。
 * 对齐：INTEGRATION_PLAN §4.3 · docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md
 *
 * **Host 约束**：仅 main.cpp `RunSimFusedSingleLaunch` 调用；CPU tikicpu **不得** launch（MIX 串行死锁）。
 *
 * FSM 阶段（CrossCore 仅 AIC↔AIV，无 AIV↔AIV）：
 *   行 16–17 NTT(y): AIV_SPLIT → AIC_MMAD → AIV_PACK → y_hat GM
 *   行 2 decode:     AIV0 ByteDecode₁₂(ek)→t_hat UB（默认标量 F203_BYTE_DECODE12_IMPL=0）
 *   行 18 内积:      kP=5 uTr pad→8 驻留 UB（AIV0 [û0,û1,tr̂,0]；AIV1 [û2,û3,0,0]）
 *   GATE:            ST_IP_AIV_DONE=4 → ST_AT_JP_GATE=8 → 释放 INTT MMAD
 *   行 19/21 INTT:   k=8 batch → u[0..3]+e₁ / v(tr̂ 行)+e₂；flag **1/3**（禁止 flag 2）
 */
#if !defined(ASCENDC_CPU_DEBUG) && ALG11_MEM_OPS == 1
#include "f203_encrypt_alg11_rom_weak.hpp"
#endif
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_encrypt_at_jp.hpp"
#include "f203_encrypt_intt_stage.hpp"
#include "f203_l18_l19_tiling.h"
#include "byte_decode12_config.hpp"
#include "f203_byte_codec/byte_decode12_vec.hpp"
#include "f203_mod_q/mod_q_add.hpp"
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
    TR_AIV_DECODE_T = 13,
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
    TR_AIV_V_DONE = 14,
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

/**
 * 四路 MMAD 一轮：lo/hi × even/odd LUT → MAT_C_TMP_*。
 * @param lutEvenTop / lutOddTop NTT 或 INTT stacked LUT 基址
 * @param mRows NTT 用 nttMRowsLogic，INTT pad-8 用 inttMRowsLogic
 */
__aicore__ inline void AicMmadRound(GM_ADDR ws, uint32_t coeffN, size_t lutEvenTop, size_t lutOddTop, uint16_t mRows)
{
    using namespace tiling;
    AicMmad mmad(mRows, coeffN, static_cast<uint16_t>(halfN));
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

/**
 * SIM 融合 MIX：行 2 + 16–17 + 18 + 19 + 21（无 μ）。
 *
 * @param uOut / vOut 时域 u、v；@param ySrc 时域 y；@param yHat ŷ 中间
 * @param uNtt / uTr 对拍用 û 与 uTr[5]；@param aHat Â；@param ekPke 公钥字节
 * @param tHat / trHatNtt 可选 dump；@param e1 / e2 噪声；@param ws workspace
 * @param traceGm 可选 16 槽进度（判死锁）；前置：host 已装 NTT+INTT LUT
 */
extern "C" __global__ __aicore__ void f203_encrypt_l18_l19(GM_ADDR uOut, GM_ADDR vOut, GM_ADDR ySrc, GM_ADDR yHat,
                                                           GM_ADDR uNtt, GM_ADDR uTr, GM_ADDR aHat, GM_ADDR ekPke,
                                                           GM_ADDR tHat, GM_ADDR trHatNtt, GM_ADDR e1, GM_ADDR e2,
                                                           GM_ADDR ws, TilingData tiling, GM_ADDR traceGm)
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
        AicMmadRound(ws, coeffN, LUT_NTT_EVEN_TOP, LUT_NTT_ODD_TOP, static_cast<uint16_t>(nttMRowsLogic));
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
        AicMmadRound(ws, coeffN, LUT_INTT_EVEN_STACKED, LUT_INTT_ODD_STACKED,
                     static_cast<uint16_t>(inttMRowsLogic));
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

        /* ── 行 18/19：kP=5 内积 uTr pad→8 驻留 UB → INTT k=8 ── */
        {
            FusedTraceMark(traceGm, TR_AIV_AT_JP_START, aic, subBlockID);
            const int32_t pBegin = subBlockID * 2;
            const int32_t pEnd = pBegin + 2;
            const uint32_t ubElems = tiling::kInttPolysPerAiv * coeffN;

            AscendC::TPipe ipPipe;
            AscendC::TQue<AscendC::TPosition::VECIN, 1> queUbU;
            ipPipe.InitBuffer(queUbU, 1, ubElems * sizeof(int32_t));
            AscendC::LocalTensor<int32_t> ubUTr = queUbU.AllocTensor<int32_t>();

            const int32_t doTrHat = (subBlockID == 0);
#if !defined(ASCENDC_CPU_DEBUG)
            if (doTrHat) {
                encrypt_at_jp::EncryptAtJpHalfRowsVec op;
                op.Init(aHat, yHat, pBegin, pEnd);

                if (ekPke != nullptr) {
                    AscendC::LocalTensor<int32_t> tHatUb = op.THatUb();
                    constexpr uint32_t kPolyBytes = f203_byte_codec::kDecodePolyBytes;
                    constexpr int32_t kCoeffN = encrypt_at_jp::kN;
                    AscendC::GlobalTensor<uint8_t> ekGm;
                    ekGm.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ekPke),
                                         static_cast<uint32_t>(encrypt_at_jp::kK) * kPolyBytes);
#if F203_BYTE_DECODE12_IMPL >= 1
                    AscendC::LocalTensor<int32_t> decodeWs = op.Decode12WsUb();
                    for (int32_t j = 0; j < encrypt_at_jp::kK; ++j) {
                        AscendC::LocalTensor<int32_t> tRowUb =
                            tHatUb[static_cast<uint32_t>(j) * static_cast<uint32_t>(kCoeffN)];
                        const uint32_t byteOff = static_cast<uint32_t>(j) * kPolyBytes;
                        f203_byte_codec::poly_byte_decode12_alg7_gm(tRowUb, ekGm, byteOff, decodeWs);
                    }
#else
                    for (int32_t j = 0; j < encrypt_at_jp::kK; ++j) {
                        AscendC::LocalTensor<int32_t> tRowUb =
                            tHatUb[static_cast<uint32_t>(j) * static_cast<uint32_t>(kCoeffN)];
                        const __gm__ uint8_t *ekRow =
                            reinterpret_cast<const __gm__ uint8_t *>(ekPke) + static_cast<uint32_t>(j) * kPolyBytes;
                        f203_byte_codec::poly_byte_decode12_scalar_gm(tRowUb, ekRow, kCoeffN);
                    }
#endif
                    AscendC::PipeBarrier<PIPE_ALL>();
                    FusedTraceMark(traceGm, TR_AIV_DECODE_T, aic, subBlockID);

                    if (tHat != nullptr) {
                        AscendC::GlobalTensor<int32_t> tGm;
                        tGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(tHat),
                                            encrypt_at_jp::kK * encrypt_at_jp::kN);
                        AscendC::DataCopy(tGm[0], tHatUb,
                                          static_cast<uint32_t>(encrypt_at_jp::kK * encrypt_at_jp::kN));
                        AscendC::PipeBarrier<PIPE_ALL>();
                    }

                    op.ProcessToUbMaybeTrHat(ubUTr, /*tHatGm*/ nullptr, trHatNtt, /*doTrHat*/ true, &tHatUb,
                                             /*unifiedUTrPad8*/ true);
                } else {
                    op.ProcessToUb(ubUTr);
                }
            } else {
                encrypt_at_jp::innerproduct_halfrows_to_ub_maybe_trhat(aHat, yHat, ubUTr, pBegin, pEnd, tHat, trHatNtt,
                                                                      /*doTrHat*/ false, nullptr,
                                                                      /*unifiedUTrPad8*/ true);
            }
#else
            encrypt_at_jp::innerproduct_halfrows_to_ub_maybe_trhat(aHat, yHat, ubUTr, pBegin, pEnd, tHat, trHatNtt,
                                                                  /*doTrHat*/ false, nullptr, /*unifiedUTrPad8*/ true);
#endif
            AscendC::PipeBarrier<PIPE_ALL>();
            FusedTraceMark(traceGm, TR_AIV_AT_JP_DONE, aic, subBlockID);

            encrypt_at_jp::dump_u_ntt_halfrows_ub(uNtt, ubUTr, pBegin, pEnd);
            if (uTr != nullptr) {
                encrypt_at_jp::dump_u_tr_pad8_ub(uTr, ubUTr, subBlockID);
            }

            {
                encrypt_intt::AivInttK8Split splitIntt(subBlockID, coeffN);
                splitIntt.Init(ws + S0, uNtt);
                splitIntt.ProcessFromLocal(ubUTr);
                AscendC::PipeBarrier<PIPE_ALL>();
                FusedTraceMark(traceGm, TR_AIV_INTT_SPLIT, aic, subBlockID);
            }

            queUbU.FreeTensor(ubUTr);
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
            encrypt_intt::AivInttK8PackMatCPlanar packIntt(subBlockID, coeffN);
            packIntt.Init(ws + MAT_C_PLANAR, ws + MAT_C_TMP_LO_EVEN, ws + MAT_C_TMP_LO_ODD, ws + MAT_C_TMP_HI_EVEN,
                          ws + MAT_C_TMP_HI_ODD);
            packIntt.Process();
            KYBER_PIPE_ALL();
        }
        {
            encrypt_intt::AivInttK8RouteUV mergeIntt(subBlockID, coeffN);
            mergeIntt.Init(uOut, vOut, ws + MAT_C_PLANAR);
            mergeIntt.Process();
            KYBER_PIPE_ALL();
        }
        FusedTraceMark(traceGm, TR_AIV_INTT_U, aic, subBlockID);

        AscendC::PipeBarrier<PIPE_ALL>();
        f203_mod_q::mod_q_add_gm_halfrows(uOut, uOut, e1, subBlockID, encrypt_at_jp::kN, encrypt_at_jp::kQ);
        KYBER_PIPE_ALL();
        FusedTraceMark(traceGm, TR_AIV_E1_DONE, aic, subBlockID);

        if (subBlockID == 0 && e2 != nullptr && vOut != nullptr) {
            f203_mod_q::mod_q_add_gm_single_row(vOut, vOut, e2, encrypt_at_jp::kQ, encrypt_at_jp::kN);
            KYBER_PIPE_ALL();
            FusedTraceMark(traceGm, TR_AIV_V_DONE, aic, subBlockID);
        }
    }
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_encrypt_l18_l19_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uOut, uint8_t *vOut,
                                        uint8_t *ySrc, uint8_t *yHat, uint8_t *uNtt, uint8_t *uTr, uint8_t *aHat,
                                        uint8_t *ekPke, uint8_t *tHat, uint8_t *trHatNtt, uint8_t *e1, uint8_t *e2,
                                        uint8_t *ws, uint8_t *tiling, uint8_t *traceGm)
{
    f203_encrypt_l18_l19<<<blockDim, l2ctrl, stream>>>(uOut, vOut, ySrc, yHat, uNtt, uTr, aHat, ekPke, tHat, trHatNtt,
                                                       e1, e2, ws, reinterpret_cast<TilingData *>(tiling), traceGm);
}
#endif
