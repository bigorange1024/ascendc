/**
 * @file f203_kem_enc_prep_ntt_entry.cpp
 * @brief Encaps 默认 Launch-1：KEM 头 + Encrypt prep + NTT(y)（每 MIX 一轮 Cube）。
 *
 * 2026-09-03：2-launch 安全路径（缓解实机多跑粘性）—
 *   Launch-1 本核：头+Â/CBD + NTT 一轮 Cube → yHat
 *   Launch-2 `f203_encrypt_l18_l19(ySrc=nullptr)`：at_jp+INTT+pack（再一轮 Cube）
 *
 * 同步（对齐 Decrypt Phase-D / fused，SIM 必需）：
 *   SoftSyncArrive（AIV0 完成 prep）→ CrossCore GATE(4/8) → NTT(1/3)
 *
 * 调试：Host 在 softSyncGm[2] 写 1 时本核跳过 NTT（仅验证 Encrypt prep∈MIX；
 * Host 须再 launch `ntt_y`）。旧 3-launch：`F203_ENCAPS_SPLIT_PREP=1`。
 * 旧双 Cube：`F203_ENCAPS_FUSED_L18=1`。
 *
 * 未采用：Host 3-launch（prep|ntt|l18）作默认 — 用户认为 prep 无 Cube 不应独占 launch。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "f203_a_hat16_config.h"
#include "f203_encrypt_full_layout.h"
#include "f203_encrypt_prep_ub.hpp"
#include "f203_kem_enc_init.hpp"
#include "f203_l18_l19_tiling.h"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

namespace {

enum FsmState : uint16_t {
    ST_AIV_SPLIT = 1,
    ST_AIC_MMAD = 2,
    ST_AIV_PACK = 3,
    ST_AIV_DONE = 4,
    ST_GATE = 8,
};

/** softSyncGm int32[3+]；slot0=prep 完成；slot2=Host 诊断 skipNtt（1=仅 prep）。 */
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

__aicore__ inline void SoftSyncClear(GM_ADDR softSyncGm, int32_t slot, int32_t subBlockID)
{
    if (subBlockID == 0) {
        reinterpret_cast<__gm__ int32_t *>(softSyncGm)[slot] = 0;
        AscendC::PipeBarrier<PIPE_ALL>();
    }
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

}  // namespace

/**
 * @param ek_gm/m_gm/K_gm/r_gm          KEM 头 + Encrypt coins(r)
 * @param a_hat_gm/prf_out_gm/re_gm/tiling_gm  Encrypt prep
 * @param yHatGm/nttWsGm/softSyncGm/tiling     NTT(y)；softSync[2]==1 时跳过 NTT
 */
extern "C" __global__ __aicore__ void f203_kem_enc_prep_ntt(
    GM_ADDR ek_gm, GM_ADDR m_gm, GM_ADDR K_gm, GM_ADDR r_gm, GM_ADDR a_hat_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
    GM_ADDR tiling_gm, GM_ADDR yHatGm, GM_ADDR nttWsGm, GM_ADDR softSyncGm, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool aic = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    tiling.tileLength = static_cast<int32_t>(::tiling::n);
    tiling.mixPass = 3;
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    const int32_t skipNtt = reinterpret_cast<__gm__ int32_t *>(softSyncGm)[2];

    /* —— prep：AIV0 串行头+两片 Â/CBD；AIV1 SoftSync 等待 —— */
    if (!aic) {
        if (subBlockID == 0) {
            const __gm__ uint8_t *ekPtr = reinterpret_cast<const __gm__ uint8_t *>(ek_gm);
            const __gm__ uint8_t *rPtr = reinterpret_cast<const __gm__ uint8_t *>(r_gm);
            F203KemEnc::KemEncInitHead(reinterpret_cast<__gm__ uint8_t *>(ek_gm),
                                       reinterpret_cast<__gm__ uint8_t *>(m_gm),
                                       reinterpret_cast<__gm__ uint8_t *>(K_gm),
                                       reinterpret_cast<__gm__ uint8_t *>(r_gm));
            AscendC::PipeBarrier<PIPE_ALL>();
            F203EncryptPrep::BuildEncryptPrepSinglePipe(
                ekPtr, rPtr, 0U, reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                reinterpret_cast<__gm__ uint8_t *>(prf_out_gm), reinterpret_cast<__gm__ int32_t *>(re_gm), tiling_gm);
            F203EncryptPrep::BuildEncryptPrepSinglePipe(
                ekPtr, rPtr, 1U, reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                reinterpret_cast<__gm__ uint8_t *>(prf_out_gm), reinterpret_cast<__gm__ int32_t *>(re_gm), tiling_gm);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
        SoftSyncArrive(softSyncGm, 0, subBlockID);
    } else if (skipNtt != 0) {
        /* 诊断 skipNtt：AIC 无 CrossCore，须 GM 自旋等 prep，避免先于 AIV 退核 */
        auto *s = reinterpret_cast<__gm__ int32_t *>(softSyncGm);
        while (s[0] == 0) {
        }
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    if (skipNtt != 0) {
        /* 诊断：Encrypt prep∈MIX 后直接退核；Host 再 launch ntt_y */
        AscendC::PipeBarrier<PIPE_ALL>();
        return;
    }

    /* —— GATE：通知 AIC prep 齐步，再开 NTT（与 Decrypt fused 同构）—— */
    if (aic) {
        FsmWait(ST_AIV_DONE);
        FsmSet(ST_GATE);
        FsmWait(ST_AIV_SPLIT);
        AicMmad mmad(static_cast<uint16_t>(mRowsLogic), coeffN, static_cast<uint16_t>(halfN));
        mmad.Init();
        mmad.Process(nttWsGm + MAT_C_TMP_LO_EVEN, nttWsGm + S0, nttWsGm + LUT_NTT_EVEN_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(nttWsGm + MAT_C_TMP_LO_ODD, nttWsGm + S0, nttWsGm + LUT_NTT_ODD_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(nttWsGm + MAT_C_TMP_HI_EVEN, nttWsGm + S0, nttWsGm + LUT_NTT_EVEN_BOTTOM);
        KYBER_PIPE_ALL();
        mmad.Process(nttWsGm + MAT_C_TMP_HI_ODD, nttWsGm + S0, nttWsGm + LUT_NTT_ODD_BOTTOM);
        KYBER_PIPE_ALL();
        FsmSet(ST_AIV_PACK);
    } else {
        FsmSet(ST_AIV_DONE);
        FsmWait(ST_GATE);
        SoftSyncClear(softSyncGm, 0, subBlockID);
        KYBER_PIPE_ALL();

        GM_ADDR ySrc = re_gm + F203EncryptFull::kReYByteOff;
        {
            AivK8Split split(subBlockID, coeffN);
            split.Init(nttWsGm + S0, ySrc);
            split.Process();
            KYBER_PIPE_ALL();
            FsmSet(ST_AIV_SPLIT);
        }
        FsmWait(ST_AIV_PACK);
        {
            AivK8PackMatCPlanar pack(subBlockID, coeffN);
            pack.Init(nttWsGm + MAT_C_PLANAR, nttWsGm + MAT_C_TMP_LO_EVEN, nttWsGm + MAT_C_TMP_LO_ODD,
                      nttWsGm + MAT_C_TMP_HI_EVEN, nttWsGm + MAT_C_TMP_HI_ODD);
            pack.Process();
            KYBER_PIPE_ALL();
        }
        {
            AivK8RouteAMod merge(subBlockID, coeffN);
            merge.Init(yHatGm, nttWsGm + MAT_C_PLANAR);
            merge.Process();
            KYBER_PIPE_ALL();
        }
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_kem_enc_prep_ntt_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ek_gm, uint8_t *m_gm,
                                         uint8_t *K_gm, uint8_t *r_gm, uint8_t *a_hat_gm, uint8_t *prf_out_gm,
                                         uint8_t *re_gm, uint8_t *tiling_gm, uint8_t *yHatGm, uint8_t *nttWsGm,
                                         uint8_t *softSyncGm, uint8_t *tiling)
{
    f203_kem_enc_prep_ntt<<<blockDim, l2ctrl, stream>>>(ek_gm, m_gm, K_gm, r_gm, a_hat_gm, prf_out_gm, re_gm,
                                                          tiling_gm, yHatGm, nttWsGm, softSyncGm,
                                                          reinterpret_cast<TilingData *>(tiling));
}
#endif
