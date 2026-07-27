/**
 * @file f203_encrypt_l18_l19_kernel.cpp
 * @brief Decaps 探针本地覆盖：Alg.14（Encrypt 行 18–24）SIM 融合 MIX 核 + 内联 tail pack；可选同核 Alg.18 FO（T19i）。
 *
 * ## 流水线位置
 * - Decaps 全链 Phase-E 的重加密段：输入 m'、coins、e₁/e₂ 等，经 NTT→内积→INTT→加噪→pack 得到 c'。
 * - 本 TU **仅**由 `exp-fips203-mlkem-kem-decaps-ct-k2` CMake 编译；**禁止**回写共享 Encrypt/D21 delivery 树。
 *
 * ## 与 Encaps 共享核的差异（本文件为探针本地覆盖）
 * | 项 | D14 k2 Encrypt 共享核 | 本 Decaps 覆盖 |
 * |----|----------------------------------------|----------------|
 * | FO | 无；pack 后由 host 另 launch 或独立 fo_only 核 | pack 尾 **同核** 内联 `KemDecFo`（T19i） |
 * | 入口指针 | 无 cInGm/zGm/KprimeGm/KoutGm | 四指针皆非空才启用 FO |
 * | 编译范围 | 全局 Encrypt 用例 | 仅本探针 vendored 副本 |
 * | CPU twin | 可走本核 | `ASCENDC_CPU_DEBUG` 下跳过 FO（防 SyncAll 挂死） |
 *
 * ## FSM（CrossCore AIC↔AIV，与 Encrypt l18_l19 同构）
 * - ST_NTT_AIV_SPLIT(1)：AIV Stage1 split；AIC **等待**后 MMAD；AIC **SET**→AIV **等待** pack。
 * - ST_IP_AIV_DONE(4)：双 AIV 完成 at_jp/INTT-S1 后 **双端 SET**（禁止仅 subBlock0 SET）。
 * - ST_AT_JP_GATE(8)：AIC 确认内积段结束，释放 INTT Stage2 的 AIC MMAD。
 *
 * ## pack 分片
 * `tail_pack_shard_gm(uOut,vOut,cGm,subBlockID)`：各 AIV 按 subBlockID 写 c' 片段到 GM；
 * 双 AIV 均完成后须 `SyncAll<isAIVOnly>` 再 FO（PipeBarrier 不能等对端 AIV）。
 *
 * ## 与 golden
 * 全链 I/O 对拍 `output/K.bin`（合法路径 = encaps K；Gate E3 拒绝 = J(z‖c)）。
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
#include "f203_mu_embed.hpp"
#include "f203_tail_pack_ops.hpp"
#include "f203_kem_dec_fo.hpp"
#include "f203_encrypt_tail_layout.h"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

/**
 * MIX 核 CrossCore 握手状态（flag 编号与 Encrypt l18_l19 一致）。
 * AIC 与双 AIV 通过 SetFlag/WaitFlag 交替推进 NTT、内积、INTT 三段。
 */
enum FsmState : uint16_t {
    /** AIV：Stage1 split 完成 → SET；AIC：WAIT 后进 MMAD */
    ST_NTT_AIV_SPLIT = 1,
    /** AIC：四次 MMAD（lo/hi × even/odd）完成后 SET，唤醒 AIV pack */
    ST_NTT_AIC_MMAD = 2,
    /** AIV：WAIT 后 pack + RouteA merge；INTT 段复用同一 flag 编号 */
    ST_NTT_AIV_PACK = 3,
    /** 双 AIV 完成 at_jp 与 INTT-S1 后均 SET；AIC WAIT 表示内积段结束 */
    ST_IP_AIV_DONE = 4,
    /**
     * AIC 在内积 WAIT 结束后 SET；双 AIV WAIT 后进入 INTT Stage2。
     * 背景：qa §11.4 / INTEGRATION_PLAN §4.3——须等 at_jp 全完再释放 INTT MMAD。
     */
    ST_AT_JP_GATE = 8,
};

#ifdef ASCENDC_CPU_DEBUG
/** CPU 孪生：强制 mix pass=3（全链路），与 SIM 默认一致 */
volatile int g_f203_l18_l19_mix_pass = 3;
#endif

/**
 * Host 可轮询的融合 trace 槽位（traceGm[k]=1 表示该阶段已到达）。
 * 仅 AIV subBlock0 写入，避免双 AIV 竞争写同一字。
 */
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
    TR_AIV_MU_E2 = 15,
    TR_COUNT = 16,
};

/**
 * 在 traceGm 标记融合阶段进度（调试用）。
 * @param traceGm 可为 nullptr（跳过）；非空时仅 AIV subBlock0 写
 */
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

/** CrossCore 等待对端 SET 到状态 st；前后 PIPE_ALL 保证内存可见性 */
__aicore__ inline void FsmWait(FsmState st, const bool aic, const int32_t subBlockID)
{
    (void)aic;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/** CrossCore 通知对端当前阶段完成，进入状态 st */
__aicore__ inline void FsmSet(FsmState st, const bool aic, const int32_t subBlockID)
{
    (void)aic;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

/**
 * AIC 侧四轮 MMAD：lo/hi 行 × even/odd LUT 列。
 * @param ws workspace GM（含 S0、MAT_C_TMP_*、LUT 偏移由 tiling 常量给出）
 * @param coeffN poly 系数个数（256）
 * @param lutEvenTop / lutOddTop 偶/奇列 LUT 在 ws 中的起始偏移
 * @param mRows MMAD 逻辑行数（NTT 或 INTT 的 mRows）
 */
__aicore__ inline void AicMmadRound(GM_ADDR ws, uint32_t coeffN, size_t lutEvenTop, size_t lutOddTop, uint16_t mRows)
{
    using namespace tiling;
    AicMmad mmad(mRows, coeffN, static_cast<uint16_t>(halfN));
    mmad.Init();
    // 四轮：tmp_lo_even / tmp_lo_odd / tmp_hi_even / tmp_hi_odd
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
 * Launch 前缀（仅 AIV0）：Alg.14 行 20–21 的 μ 嵌入。
 * 行 20：μ ← ByteDecode₁₂⁻¹(m)；行 21：e₂' = e₂ + μ (mod q) 写回 e₂ GM。
 * 后续 v ← INTT(tr̂) + e₂' 即标准 v ← INTT(tr̂) + μ + e₂。
 *
 * @param mGm 32B 消息 m'（F203_TAIL_MSG_BYTES）
 * @param e2Gm 256×int32 噪声 e₂；本函数原地更新为 e₂'
 */
__aicore__ inline void PrefixEmbedMuIntoE2Gm(GM_ADDR mGm, GM_ADDR e2Gm, int32_t coeffN, int32_t q)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queM;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufE2;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufMu;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufT1;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufT2;
    const uint32_t lineBytes = static_cast<uint32_t>(coeffN) * sizeof(int32_t);
    pipe.InitBuffer(queM, 1, 64U);
    pipe.InitBuffer(bufE2, lineBytes);
    pipe.InitBuffer(bufMu, lineBytes);
    pipe.InitBuffer(bufT1, lineBytes);
    pipe.InitBuffer(bufT2, lineBytes);

    AscendC::LocalTensor<uint8_t> mLocal = queM.AllocTensor<uint8_t>();
    AscendC::LocalTensor<int32_t> e2Ub = bufE2.Get<int32_t>();
    AscendC::LocalTensor<int32_t> muUb = bufMu.Get<int32_t>();
    AscendC::LocalTensor<int32_t> t1 = bufT1.Get<int32_t>();
    AscendC::LocalTensor<int32_t> t2 = bufT2.Get<int32_t>();

    AscendC::GlobalTensor<uint8_t> gmM;
    AscendC::GlobalTensor<int32_t> gmE2;
    gmM.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(mGm), F203_TAIL_MSG_BYTES);
    gmE2.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(e2Gm), static_cast<uint32_t>(coeffN));

    // GM → UB：读 m，解码得 μ 系数向量
    AscendC::DataCopy(mLocal, gmM, F203_TAIL_MSG_BYTES);
    AscendC::PipeBarrier<PIPE_ALL>();
    f203_tail::mu_embed_from_message_ub(mLocal, muUb);
    AscendC::PipeBarrier<PIPE_ALL>();
    // 读 e₂，加 μ 后写回 GM（双 AIV 后续从 GM 读已更新的 e₂'）
    AscendC::DataCopy(e2Ub, gmE2, static_cast<uint32_t>(coeffN));
    AscendC::PipeBarrier<PIPE_ALL>();
    f203_mod_q::mod_q_add_ub_inplace(e2Ub, muUb, q, t1, t2, coeffN);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmE2, e2Ub, static_cast<uint32_t>(coeffN));
    AscendC::PipeBarrier<PIPE_ALL>();
    queM.FreeTensor(mLocal);
}

/**
 * Alg.14 行 18–24 融合 MIX 入口（Decaps 探针：含 pack + 可选同核 FO）。
 *
 * @param uOut/vOut INTT 后 u/v 系数 GM（加 e₁/e₂ 前由 merge 写出，再加噪）
 * @param ySrc/yHat NTT 段源/ŷ 输出
 * @param uNtt/uTr 内积 NTT 域 / pad-8 驻留 UB 的 GM 镜像（调试）
 * @param aHat/ekPke/tHat/trHatNtt at_jp 与 t̂ 解码相关 GM
 * @param mGm 消息 m'；e1/e2 噪声；ws workspace；tiling 瓦片参数
 * @param cGm pack 输出的 c'（768B 分片写）；traceGm 可选进度
 * @param cInGm/zGm/KprimeGm/KoutGm **四者皆非空** 时 pack 尾启用 Alg.18 FO（Decaps 专用；Encaps 传 nullptr）
 */
extern "C" __global__ __aicore__ void f203_encrypt_l18_l19(GM_ADDR uOut, GM_ADDR vOut, GM_ADDR ySrc, GM_ADDR yHat,
                                                           GM_ADDR uNtt, GM_ADDR uTr, GM_ADDR aHat, GM_ADDR ekPke,
                                                           GM_ADDR tHat, GM_ADDR trHatNtt, GM_ADDR mGm, GM_ADDR e1,
                                                           GM_ADDR e2, GM_ADDR ws, TilingData tiling, GM_ADDR cGm,
                                                           GM_ADDR traceGm, GM_ADDR cInGm, GM_ADDR zGm,
                                                           GM_ADDR KprimeGm, GM_ADDR KoutGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    FsmState st;

    if (aic) {
        /* ═══════════════ AIC 分支：仅 MMAD + CrossCore 握手 ═══════════════ */

        /* ── 行 18 NTT Stage2：WAIT split(1) → MMAD → SET pack(3) ── */
        st = ST_NTT_AIV_SPLIT;
        FsmWait(st, aic, subBlockID);
        st = ST_NTT_AIC_MMAD;
        AicMmadRound(ws, coeffN, LUT_NTT_EVEN_TOP, LUT_NTT_ODD_TOP, static_cast<uint16_t>(nttMRowsLogic));
        FusedTraceMark(traceGm, TR_AIC_NTT_MMAD, aic, subBlockID);
        st = ST_NTT_AIV_PACK;
        FsmSet(st, aic, subBlockID);

        /* 内积阶段 AIC 空转：直至双 AIV SET ST_IP_AIV_DONE(4) */
        st = ST_IP_AIV_DONE;
        FsmWait(st, aic, subBlockID);
        FusedTraceMark(traceGm, TR_AIC_IP_WAIT_DONE, aic, subBlockID);
        /* 释放 INTT Stage2：SET gate(8)，唤醒 AIV 进入 INTT pack */
        st = ST_AT_JP_GATE;
        FsmSet(st, aic, subBlockID);
        FusedTraceMark(traceGm, TR_AIC_AT_JP_GATE, aic, subBlockID);

        /* ── 行 19 INTT Stage2：复用 flag 1/3（WAIT1 → MMAD → SET3）── */
        st = ST_NTT_AIV_SPLIT;
        FsmWait(st, aic, subBlockID);
        AicMmadRound(ws, coeffN, LUT_INTT_EVEN_STACKED, LUT_INTT_ODD_STACKED,
                     static_cast<uint16_t>(inttMRowsLogic));
        FusedTraceMark(traceGm, TR_AIC_INTT_MMAD, aic, subBlockID);
        st = ST_NTT_AIV_PACK;
        FsmSet(st, aic, subBlockID);
    } else {
        /* ═══════════════ AIV 分支：split/pack、at_jp、加噪、pack、FO ═══════════════ */

        /* ── 行 20/21 前缀：AIV0 将 μ 嵌入 e₂ GM；PipeBarrier 后双 AIV 可见 ── */
        if (subBlockID == 0 && mGm != nullptr && e2 != nullptr) {
            PrefixEmbedMuIntoE2Gm(mGm, e2, encrypt_at_jp::kN, encrypt_at_jp::kQ);
            FusedTraceMark(traceGm, TR_AIV_MU_E2, aic, subBlockID);
        }
        AscendC::PipeBarrier<PIPE_ALL>();

        /* ── 行 18 NTT Stage1：y → workspace S0 ── */
        {
            st = ST_NTT_AIV_SPLIT;
            AivK8Split splitNtt(subBlockID, coeffN);
            splitNtt.Init(ws + S0, ySrc);
            splitNtt.Process();
            KYBER_PIPE_ALL();
            FsmSet(st, aic, subBlockID);
            FusedTraceMark(traceGm, TR_AIV_NTT_SPLIT, aic, subBlockID);
        }

        /* ── 行 18 NTT Pack：独立 TPipe 作用域，避免与 merge 争用 UB ── */
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
            // RouteA：平面 mat_c → ŷ_hat 写 yHat GM
            AivK8RouteAMod mergeNtt(subBlockID, coeffN);
            mergeNtt.Init(yHat, ws + MAT_C_PLANAR);
            mergeNtt.Process();
            KYBER_PIPE_ALL();
        }
        FusedTraceMark(traceGm, TR_AIV_NTT_YHAT, aic, subBlockID);

        /* ★ SYNC-ŷ：内积读 ŷ 前须保证 NTT pack 全完 */
        KYBER_PIPE_ALL();

        /* ── 行 18/19 内积段：k2 的两个 u poly 均由 AIV0 处理，AIV1 只负责 tr̂/v ── */
        {
            FusedTraceMark(traceGm, TR_AIV_AT_JP_START, aic, subBlockID);
            // ML-KEM-512：AIV0 负责 u0/u1，AIV1 负责 v 的 tr̂；禁止沿用 k3 的第三个 u 行。
            const int32_t pBegin = (subBlockID == 0) ? 0 : 2;
            const int32_t pEnd = (subBlockID == 0) ? 2 : 2;
            const uint32_t ubElems = tiling::kInttPolysPerAiv * coeffN;

            AscendC::TPipe ipPipe;
            AscendC::TQue<AscendC::TPosition::VECIN, 1> queUbU;
            ipPipe.InitBuffer(queUbU, 1, ubElems * sizeof(int32_t));
            AscendC::LocalTensor<int32_t> ubUTr = queUbU.AllocTensor<int32_t>();

            // k2 D14 基线：仅 AIV1 解码 ek→t̂ 并计算 tr̂；其 INTT 分片同时产出 v。
            const int32_t doTrHat = (subBlockID == 1);
#if !defined(ASCENDC_CPU_DEBUG)
            if (doTrHat) {
                encrypt_at_jp::EncryptAtJpHalfRowsVec op;
                op.Init(aHat, yHat, pBegin, pEnd);

                if (ekPke != nullptr) {
                    // Decaps：从 ek_pke ByteDecode₁₂ 得 t̂，再 at·ŷ + tr̂
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

            // 调试镜像：u_ntt 半行 + u_tr pad8
            encrypt_at_jp::dump_u_ntt_halfrows_ub(uNtt, ubUTr, pBegin, pEnd);
            if (uTr != nullptr) {
                encrypt_at_jp::dump_u_tr_pad8_ub(uTr, ubUTr, subBlockID);
            }

            // INTT Stage1：从 UB 的 u_ntt 分片写入 ws S0（与后续 AIC MMAD 衔接）
            {
                encrypt_intt::AivInttK8Split splitIntt(subBlockID, coeffN);
                splitIntt.Init(ws + S0, uNtt);
                splitIntt.ProcessFromLocal(ubUTr);
                AscendC::PipeBarrier<PIPE_ALL>();
                FusedTraceMark(traceGm, TR_AIV_INTT_SPLIT, aic, subBlockID);
            }

            queUbU.FreeTensor(ubUTr);
        }

        /*
         * 双 AIV **均** SET ST_IP_AIV_DONE(4)。
         * 禁止仅 subBlock0 SET——否则 AIC 提前进 INTT MMAD，与对端 at_jp 竞态。
         */
        st = ST_IP_AIV_DONE;
        FsmSet(st, aic, subBlockID);
        if (subBlockID == 0) {
            FusedTraceMark(traceGm, TR_AIV_IP_SIGNAL, aic, subBlockID);
        }

        /* WAIT gate(8)：AIC 已确认内积段结束，可安全进入 INTT Stage2 */
        st = ST_AT_JP_GATE;
        FsmWait(st, aic, subBlockID);
        FusedTraceMark(traceGm, TR_AIV_AT_JP_GATE, aic, subBlockID);
        KYBER_PIPE_ALL();

        /* INTT Stage2：S0 已含 Stage1 结果；SET split(1) 唤醒 AIC MMAD */
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
            // merge：平面 mat_c → uOut（4 行）+ vOut（1 行）
            encrypt_intt::AivInttK8RouteUV mergeIntt(subBlockID, coeffN);
            mergeIntt.Init(uOut, vOut, ws + MAT_C_PLANAR);
            mergeIntt.Process();
            KYBER_PIPE_ALL();
        }
        FusedTraceMark(traceGm, TR_AIV_INTT_U, aic, subBlockID);

        /* 行 19：u ← u + e₁ (mod q)，k2 仅 AIV0 覆盖两行 u；AIV1 无 u 行 */
        AscendC::PipeBarrier<PIPE_ALL>();
        {
            const int32_t pBegin = (subBlockID == 0) ? 0 : 2;
            const int32_t pEnd = (subBlockID == 0) ? 2 : 2;
            if (pBegin < pEnd) {
                f203_mod_q::mod_q_add_gm_polyrows(uOut, uOut, e1, encrypt_at_jp::kQ, pBegin, pEnd,
                                                  encrypt_at_jp::kN);
            }
        }
        KYBER_PIPE_ALL();
        FusedTraceMark(traceGm, TR_AIV_E1_DONE, aic, subBlockID);

        /* 行 19：v ← v + e₂' (mod q)；vOut 由 AIV1 scatter 得到，继续由 AIV1 累加避免竞态 */
        if (subBlockID == 1 && e2 != nullptr && vOut != nullptr) {
            f203_mod_q::mod_q_add_gm_single_row(vOut, vOut, e2, encrypt_at_jp::kQ, encrypt_at_jp::kN);
            KYBER_PIPE_ALL();
            FusedTraceMark(traceGm, TR_AIV_V_DONE, aic, subBlockID);
        }

        /*
         * 行 22–24：内联 tail pack（SIM 单 launch，取代独立 pack 核）。
         * cGm!=nullptr 时各 AIV 按 subBlockID 分片写 c' 到 GM 不同区间。
         */
        f203_tail::tail_pack_shard_gm(uOut, vOut, cGm, subBlockID);

        /*
         * T19i：Alg.18 FO 收回本核尾部（取代独立 fo_only launch）。
         *
         * 启用条件（Decaps 专用，Encaps 共享核四指针为 nullptr）：
         *   cGm、cInGm、zGm、KprimeGm、KoutGm **皆非空**，且 subBlockID==0。
         *
         * 同步：双 AIV pack 分片均写完 GM 后须 SyncAll<isAIVOnly>；
         *       PipeBarrier 不能等待对端 AIV 完成 pack。
         * CPU twin（ASCENDC_CPU_DEBUG）不 launch 本核 FO 路径，走 host pack_fo 拆分。
         */
#ifndef ASCENDC_CPU_DEBUG
        AscendC::SyncAll</*isAIVOnly=*/true>();
        if (subBlockID == 0 && cGm != nullptr && cInGm != nullptr && zGm != nullptr && KprimeGm != nullptr &&
            KoutGm != nullptr) {
            AscendC::PipeBarrier<PIPE_ALL>();
            // KemDecFo：比较 c' 与 c_in → K'；恒定时间选 K' 或 J(z‖c) 写 Kout
            F203KemDec::KemDecFo(reinterpret_cast<__gm__ uint8_t *>(cInGm), reinterpret_cast<__gm__ uint8_t *>(cGm),
                                 reinterpret_cast<__gm__ uint8_t *>(zGm), reinterpret_cast<__gm__ uint8_t *>(KprimeGm),
                                 reinterpret_cast<__gm__ uint8_t *>(KoutGm));
        }
#endif
    }
}

#ifndef __CCE_KT_TEST__
/** Host 侧 kernel launch 包装（blockDim/stream 由 main 传入） */
extern "C" void f203_encrypt_l18_l19_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uOut, uint8_t *vOut,
                                        uint8_t *ySrc, uint8_t *yHat, uint8_t *uNtt, uint8_t *uTr, uint8_t *aHat,
                                        uint8_t *ekPke, uint8_t *tHat, uint8_t *trHatNtt, uint8_t *mGm, uint8_t *e1,
                                        uint8_t *e2, uint8_t *ws, uint8_t *tiling, uint8_t *cGm, uint8_t *traceGm,
                                        uint8_t *cInGm, uint8_t *zGm, uint8_t *KprimeGm, uint8_t *KoutGm)
{
    f203_encrypt_l18_l19<<<blockDim, l2ctrl, stream>>>(uOut, vOut, ySrc, yHat, uNtt, uTr, aHat, ekPke, tHat, trHatNtt,
                                                       mGm, e1, e2, ws, reinterpret_cast<TilingData *>(tiling), cGm,
                                                       traceGm, cInGm, zGm, KprimeGm, KoutGm);
}
#endif
