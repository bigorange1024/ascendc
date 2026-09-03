/**
 * @file f203_decrypt_g4_chain_ntt_entry.cpp
 * @brief Phase-D 默认 Launch-1：prep + NTT(u) + su_dot + pad（每 MIX 一轮 Cube）。
 *
 * 2026-09-03：2-launch 安全路径（缓解实机多跑粘性）—
 *   Launch-1 本核：unpack/decode（无 Cube）+ NTT 一轮 Cube + su_dot/pad
 *   Launch-2 `f203_decrypt_g4_chain_intt`：INTT 一轮 Cube + 尾
 * 旧单核双 Cube：`F203_DECRYPT_FUSED=1` → `f203_decrypt_device_fused`。
 *
 * 同步（对齐 fused，SIM 必需）：
 *   SoftSync 双边齐步 → CrossCore GATE(4/8) → NTT → SoftSync（等 AIV0 su_dot/pad）
 */
#include "f203_decrypt_decode_impl.hpp"
#include "f203_decrypt_layout.h"
#include "f203_decrypt_ntt_u_impl.hpp"
#include "f203_decrypt_ntt_u_tiling.h"
/* 与 fused 同链编进一库时，su_dot ROM 表须改名，否则 CPU/AIV 多定义 gSuDot* */
#if defined(ASCENDC_CPU_DEBUG) || defined(__DAV_C220_VEC__) || defined(__DAV_C310_VEC__)
#define gSuDotGammasGm                gChainNttSuDotGammasGm
#define gSuDotGatherEvenByteGm        gChainNttSuDotGatherEvenByteGm
#define gSuDotGatherOddByteGm         gChainNttSuDotGatherOddByteGm
#define gSuDotInterleaveReorderByteGm gChainNttSuDotInterleaveReorderByteGm
#endif
#include "f203_decrypt_su_dot_impl.hpp"
#include "f203_decrypt_unpack_impl.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

#ifdef ASCENDC_CPU_DEBUG
volatile int g_f203_decrypt_g4_chain_ntt_mix_pass = 3;
#endif

namespace {

enum GateState : uint16_t {
    ST_AIV_DONE = 4,
    ST_GATE = 8,
};

/** softSyncGm int32[3+]；slot0/1=prep 双边；slot2=su_dot 完成（AIV0→AIV1）。 */
__aicore__ inline void SoftSyncBarrier2(GM_ADDR softSyncGm, int32_t subBlockID)
{
    auto *s = reinterpret_cast<__gm__ int32_t *>(softSyncGm);
    if (subBlockID == 0) {
        s[0] = 1;
        AscendC::PipeBarrier<PIPE_ALL>();
        while (s[1] == 0) {
        }
        AscendC::PipeBarrier<PIPE_ALL>();
    } else {
        s[1] = 1;
        AscendC::PipeBarrier<PIPE_ALL>();
        while (s[0] == 0) {
        }
        AscendC::PipeBarrier<PIPE_ALL>();
    }
}

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

__aicore__ inline void FsmWait(GateState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

__aicore__ inline void FsmSet(GateState st)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(st);
    KYBER_PIPE_ALL();
}

}  // namespace

/**
 * @param dkGm/cGm     生产输入
 * @param uGm/vGm/sHatGm  prep 写出
 * @param uHatGm/wHatGm/wPaddedGm  NTT+su_dot 写出
 * @param nttWsGm      NTT workspace（含 LUT）
 * @param softSyncGm   AIV SoftSync 哨兵（Host 须清零；至少 3×int32）
 */
extern "C" __global__ __aicore__ void f203_decrypt_g4_chain_ntt(
    GM_ADDR dkGm, GM_ADDR cGm, GM_ADDR uGm, GM_ADDR vGm, GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm,
    GM_ADDR wPaddedGm, GM_ADDR nttWsGm, GM_ADDR softSyncGm, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t blockIdx = static_cast<int32_t>(AscendC::GetBlockIdx());
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    tiling.tileLength = static_cast<int32_t>(::tiling::n);
    tiling.kPolys = static_cast<int32_t>(::tiling::kK);
#ifdef ASCENDC_CPU_DEBUG
    tiling.mixPass = g_f203_decrypt_g4_chain_ntt_mix_pass;
#else
    tiling.mixPass = 3;
#endif

    if (AIC) {
        /* 等 AIV prep 齐步后放行 NTT（与 fused GATE 同构） */
        FsmWait(ST_AIV_DONE);
        FsmSet(ST_GATE);
        decrypt_g4::ntt_u_impl(uHatGm, uGm, nttWsGm, tiling);
    } else {
        if (subBlockID == 0 && blockIdx == 0) {
            decrypt_g4::unpack_c_impl(cGm, uGm, vGm);
            AscendC::PipeBarrier<PIPE_ALL>();
            decrypt_g4::decode_s_hat_impl(dkGm, sHatGm);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
        SoftSyncBarrier2(softSyncGm, subBlockID);

        FsmSet(ST_AIV_DONE);
        FsmWait(ST_GATE);
        KYBER_PIPE_ALL();

        decrypt_g4::ntt_u_impl(uHatGm, uGm, nttWsGm, tiling);
        AscendC::PipeBarrier<PIPE_ALL>();

        if (blockIdx == 0 && subBlockID == 0) {
            decrypt_g4::su_dot_impl(sHatGm, uHatGm, wHatGm);
            AscendC::PipeBarrier<PIPE_ALL>();
            decrypt_g4::pad_w_hat_for_intt(wPaddedGm, wHatGm);
        }
        /* AIV1 等 AIV0 写完 wPadded，再共同退核（供下一 launch 读） */
        SoftSyncArrive(softSyncGm, 2, subBlockID);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}
