/**
 * @file f203_decrypt_device_fused_entry.cpp
 * @brief FIPS 203 Alg.15 **单 kernel**：prep → NTT(u) → su_dot → INTT(ŵ) → Compress₁/Encode₁ → m。
 *
 * 对应标准步骤（ml_kem_1024 / k=4）：
 *   行 1–2  输入 dk_PKE、c（Host 已 H2D）
 *   行 3–4  c → (u',v')：ByteDecode₁₁/₅ + Decompress₁₁/₅（AIV0 unpack）
 *   行 5    ŝ ← ByteDecode₁₂(dk)（AIV0 decode）
 *   行 5'   û ← NTT(u')（双 AIV Stage1/3 + AIC MMAD）
 *   行 6    ŵ ← ⟨ŝ, û⟩（AIV0 su_dot，Alg.11 向量）
 *   行 6'   w ← INTT(ŵ)（pad 后同 NTT 流水；出 wTime）
 *   行 6–7  w ← (v'−w) mod q → Compress₁ → ByteEncode₁ → m（AIV0 尾）
 *
 * 同步：
 *   - NTT：CrossCore flag 1/2/3；INTT：flag 1/3（禁 2，对齐 Encrypt）
 *   - 段间 GATE：flag 4（AIV→AIC）+ 8（AIC→AIV）
 *   - prep / su_dot 仅 AIV0：AIV1 经 softSyncGm 哨兵等待，再双 AIV Set(4)
 *
 * 生产路径：pad→wPadded GM + INTT Process()（UB 驻留实验已回滚，见 STATUS）。
 * 须写 ::tiling::（与 AscendC::tiling 歧义，禁止 using namespace tiling）。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_decrypt_decode_impl.hpp"
#include "f203_decrypt_layout.h"
#include "f203_decrypt_ntt_u_tiling.h"
#include "f203_decrypt_su_dot_impl.hpp"
#include "f203_decrypt_tail_compress1_byteencode1.hpp"
#include "f203_decrypt_unpack_impl.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

namespace {

/** CrossCore FSM 状态（与 Encrypt l18_l19 / NTT 探针同编号语义）。 */
enum FsmState : uint16_t {
    ST_SPLIT = 1,   /* AIV Stage1 完成 → AIC MMAD */
    ST_MMAD = 2,    /* 仅 NTT 路径 AIC 内部；INTT 不用 */
    ST_PACK = 3,    /* AIC MMAD 完成 → AIV Stage3 pack/merge */
    ST_AIV_DONE = 4,/* 段末 AIV 齐步 → AIC GATE */
    ST_GATE = 8,    /* AIC 放行 → 下一段 AIV */
};

/**
 * softSyncGm：int32[2]；slot0=prep 完成，slot1=su_dot+pad 完成。
 * AIV0 写 1；AIV1 自旋等到非 0。Host 启动前须清零。
 * 可选 traceGm：int32[TR_COUNT] 段标记；默认 nullptr 不写。
 */
__aicore__ inline void SoftSyncArrive(GM_ADDR softSyncGm, int32_t slot, int32_t subBlockID)
{
    auto *s = reinterpret_cast<__gm__ int32_t *>(softSyncGm);
    if (subBlockID == 0) {
        s[slot] = 1;
        AscendC::PipeBarrier<PIPE_ALL>();
    } else {
        while (s[slot] == 0) {
        }
        AscendC::PipeBarrier<PIPE_ALL>();
    }
}

/** AIV0 清哨兵，供下一段复用同一 slot。 */
__aicore__ inline void SoftSyncClear(GM_ADDR softSyncGm, int32_t slot, int32_t subBlockID)
{
    if (subBlockID == 0) {
        reinterpret_cast<__gm__ int32_t *>(softSyncGm)[slot] = 0;
        AscendC::PipeBarrier<PIPE_ALL>();
    }
}

/** 段 TRACE：写 traceGm[stage]=1（仅 AIV0 / AIC；traceGm 可空）。 */
enum DecTraceStage : int32_t {
    TR_AIV_PREP_DONE = 0,     /* unpack+decode 后 SoftSyncArrive(0) 前 */
    TR_AIV_GATE1_WAIT = 1,    /* 第一轮 GATE Wait(8) 完成 */
    TR_AIV_NTT_SPLIT = 2,     /* NTT Stage1 Set(1) 后 */
    TR_AIV_NTT_PACK = 3,      /* NTT Stage3 pack/merge 后 */
    TR_AIV_SUDOT_DONE = 4,    /* su_dot+pad 后 SoftSyncArrive(1) 前 */
    TR_AIV_GATE2_WAIT = 5,    /* 第二轮 GATE Wait(8) 完成 */
    TR_AIV_INTT_SPLIT = 6,    /* INTT Stage1 Set(1) 后 */
    TR_AIV_INTT_PACK = 7,     /* INTT Stage3 后 */
    TR_AIV_TAIL_DONE = 8,     /* extract m 后 */
    TR_AIC_GATE1_SET = 9,     /* AIC 第一轮 Set(8) */
    TR_AIC_NTT_MMAD = 10,     /* AIC NTT MMAD 后 */
    TR_AIC_GATE2_SET = 11,    /* AIC 第二轮 Set(8) */
    TR_AIC_INTT_MMAD = 12,    /* AIC INTT MMAD 后 */
    TR_COUNT = 13,
};

__aicore__ inline void DecTraceMark(GM_ADDR traceGm, DecTraceStage stage, bool aic, int32_t subBlockID)
{
    if (traceGm == nullptr) {
        return;
    }
    /* AIV 仅 sub0 写；AIC 可写 */
    if (!aic && subBlockID != 0) {
        return;
    }
    auto *t = reinterpret_cast<__gm__ int32_t *>(traceGm);
    t[static_cast<int32_t>(stage)] = 1;
}

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

/**
 * AIC 一轮 MMAD：对 workspace 内 4 块 mat_c_tmp（lo/hi × even/odd）各乘对应 LUT 条带。
 * NTT 与 INTT 共用本函数，仅 ws 基址不同（nttWsGm / inttWsGm）。
 */
__aicore__ inline void AicMmadRound(GM_ADDR ws, uint32_t coeffN)
{
    AicMmad mmad(static_cast<uint16_t>(::tiling::mRowsLogic), coeffN, static_cast<uint16_t>(::tiling::halfN));
    mmad.Init();
    mmad.Process(ws + ::tiling::MAT_C_TMP_LO_EVEN, ws + ::tiling::S0, ws + ::tiling::LUT_EVEN_TOP);
    KYBER_PIPE_ALL();
    mmad.Process(ws + ::tiling::MAT_C_TMP_LO_ODD, ws + ::tiling::S0, ws + ::tiling::LUT_ODD_TOP);
    KYBER_PIPE_ALL();
    mmad.Process(ws + ::tiling::MAT_C_TMP_HI_EVEN, ws + ::tiling::S0, ws + ::tiling::LUT_EVEN_BOTTOM);
    KYBER_PIPE_ALL();
    mmad.Process(ws + ::tiling::MAT_C_TMP_HI_ODD, ws + ::tiling::S0, ws + ::tiling::LUT_ODD_BOTTOM);
    KYBER_PIPE_ALL();
}

} // namespace

/**
 * 设备入口：Alg.15 全链融合。
 * @param dkGm/cGm     生产输入（ByteEncode₁₂(ŝ) / c₁‖c₂）
 * @param uGm/vGm      中间：u'/v'（unpack 写出）
 * @param sHatGm       中间：ŝ
 * @param uHatGm       中间：û = NTT(u')
 * @param wHatGm       中间：ŵ = ⟨ŝ,û⟩
 * @param wPaddedGm    中间：ŵ pad 成 k=4 polyvec 前缀供 INTT Stage1
 * @param wTimeGm      中间：INTT 时域 w
 * @param mGm          生产输出：m[32]
 * @param nttWsGm/inttWsGm  NTT/INTT workspace（含 LUT）
 * @param softSyncGm   AIV0/AIV1 软同步哨兵
 * @param tiling       NTT/INTT tiling
 * @param traceGm      可选段 TRACE（int32[TR_COUNT]）；nullptr 跳过
 */
extern "C" __global__ __aicore__ void f203_decrypt_device_fused(
    GM_ADDR dkGm, GM_ADDR cGm, GM_ADDR uGm, GM_ADDR vGm, GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm,
    GM_ADDR wPaddedGm, GM_ADDR wTimeGm, GM_ADDR mGm, GM_ADDR nttWsGm, GM_ADDR inttWsGm, GM_ADDR softSyncGm,
    TilingData tiling, GM_ADDR traceGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    tiling.mixPass = 3;
    FsmState st;

    if (aic) {
        /* ========== AIC：两轮 MMAD（NTT + INTT），中间 GATE 与 AIV 握手 ========== */
        /* 等 prep 段 AIV 齐步 */
        st = ST_AIV_DONE;
        FsmWait(st);
        st = ST_GATE;
        FsmSet(st);
        DecTraceMark(traceGm, TR_AIC_GATE1_SET, aic, subBlockID);

        /* NTT：等 Stage1 → MMAD → 通知 Stage3 */
        st = ST_SPLIT;
        FsmWait(st);
        st = ST_MMAD;
        AicMmadRound(nttWsGm, coeffN);
        DecTraceMark(traceGm, TR_AIC_NTT_MMAD, aic, subBlockID);
        st = ST_PACK;
        FsmSet(st);

        /* 等 su_dot+pad 段 AIV 齐步 */
        st = ST_AIV_DONE;
        FsmWait(st);
        st = ST_GATE;
        FsmSet(st);
        DecTraceMark(traceGm, TR_AIC_GATE2_SET, aic, subBlockID);

        /* INTT：等 Stage1 → MMAD → 通知 Stage3（无 ST_MMAD 中间态） */
        st = ST_SPLIT;
        FsmWait(st);
        AicMmadRound(inttWsGm, coeffN);
        DecTraceMark(traceGm, TR_AIC_INTT_MMAD, aic, subBlockID);
        st = ST_PACK;
        FsmSet(st);
    } else {
        /* ========== AIV：prep → NTT → su_dot → INTT → 尾 ========== */

        /* --- Alg.15 行 3–5：unpack c + decode dk（仅 AIV0）--- */
        if (subBlockID == 0) {
            decrypt_g4::unpack_c_impl(cGm, uGm, vGm);
            AscendC::PipeBarrier<PIPE_ALL>();
            decrypt_g4::decode_s_hat_impl(dkGm, sHatGm);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
        DecTraceMark(traceGm, TR_AIV_PREP_DONE, aic, subBlockID);
        SoftSyncArrive(softSyncGm, 0, subBlockID);

        /* GATE：通知 AIC prep 完成，等 AIC 放行再开 NTT */
        st = ST_AIV_DONE;
        FsmSet(st);
        st = ST_GATE;
        FsmWait(st);
        SoftSyncClear(softSyncGm, 0, subBlockID);
        KYBER_PIPE_ALL();
        DecTraceMark(traceGm, TR_AIV_GATE1_WAIT, aic, subBlockID);

        /* --- û ← NTT(u')：Stage1 split → AIC MMAD → Stage3 pack + RouteA merge --- */
        {
            st = ST_SPLIT;
            AivK8Split split(subBlockID, coeffN);
            split.Init(nttWsGm + ::tiling::S0, uGm);
            split.Process();
            KYBER_PIPE_ALL();
            FsmSet(st);
            DecTraceMark(traceGm, TR_AIV_NTT_SPLIT, aic, subBlockID);
        }
        {
            st = ST_PACK;
            FsmWait(st);
            AivK8PackMatCPlanar pack(subBlockID, coeffN);
            pack.Init(nttWsGm + ::tiling::MAT_C_PLANAR, nttWsGm + ::tiling::MAT_C_TMP_LO_EVEN,
                      nttWsGm + ::tiling::MAT_C_TMP_LO_ODD, nttWsGm + ::tiling::MAT_C_TMP_HI_EVEN,
                      nttWsGm + ::tiling::MAT_C_TMP_HI_ODD);
            pack.Process();
            KYBER_PIPE_ALL();
        }
        {
            AivK8RouteAMod merge(subBlockID, coeffN);
            merge.Init(uHatGm, nttWsGm + ::tiling::MAT_C_PLANAR);
            merge.Process();
            KYBER_PIPE_ALL();
        }
        DecTraceMark(traceGm, TR_AIV_NTT_PACK, aic, subBlockID);

        /* --- ŵ ← ⟨ŝ,û⟩ + pad 供 INTT（仅 AIV0）--- */
        if (subBlockID == 0) {
            decrypt_g4::su_dot_impl(sHatGm, uHatGm, wHatGm);
            AscendC::PipeBarrier<PIPE_ALL>();
            decrypt_g4::pad_w_hat_for_intt(wPaddedGm, wHatGm);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
        DecTraceMark(traceGm, TR_AIV_SUDOT_DONE, aic, subBlockID);
        SoftSyncArrive(softSyncGm, 1, subBlockID);

        st = ST_AIV_DONE;
        FsmSet(st);
        st = ST_GATE;
        FsmWait(st);
        SoftSyncClear(softSyncGm, 1, subBlockID);
        KYBER_PIPE_ALL();
        DecTraceMark(traceGm, TR_AIV_GATE2_WAIT, aic, subBlockID);

        /* --- w ← INTT(ŵ_padded)：输入 wPaddedGm，输出 wTimeGm --- */
        {
            AivK8Split split(subBlockID, coeffN);
            split.Init(inttWsGm + ::tiling::S0, wPaddedGm);
            split.Process();
            AscendC::PipeBarrier<PIPE_ALL>();
        }
        st = ST_SPLIT;
        FsmSet(st);
        DecTraceMark(traceGm, TR_AIV_INTT_SPLIT, aic, subBlockID);

        {
            st = ST_PACK;
            FsmWait(st);
            AivK8PackMatCPlanar pack(subBlockID, coeffN);
            pack.Init(inttWsGm + ::tiling::MAT_C_PLANAR, inttWsGm + ::tiling::MAT_C_TMP_LO_EVEN,
                      inttWsGm + ::tiling::MAT_C_TMP_LO_ODD, inttWsGm + ::tiling::MAT_C_TMP_HI_EVEN,
                      inttWsGm + ::tiling::MAT_C_TMP_HI_ODD);
            pack.Process();
            KYBER_PIPE_ALL();
        }
        {
            AivK8RouteAMod merge(subBlockID, coeffN);
            merge.Init(wTimeGm, inttWsGm + ::tiling::MAT_C_PLANAR);
            merge.Process();
            KYBER_PIPE_ALL();
        }
        DecTraceMark(traceGm, TR_AIV_INTT_PACK, aic, subBlockID);

        /* --- Alg.15 行 6–7：m ← Encode₁(Compress₁(v'−w))（仅 AIV0）--- */
        if (subBlockID == 0) {
            decrypt_device::extract_m_compress1_byteencode1(vGm, wTimeGm, mGm);
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        DecTraceMark(traceGm, TR_AIV_TAIL_DONE, aic, subBlockID);
    }
}
