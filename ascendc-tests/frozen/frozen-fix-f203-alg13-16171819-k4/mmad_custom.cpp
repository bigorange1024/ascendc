/**
 * Alg.13 行 16–17–18–19（k=4）：
 *   16–17：Tag5T MIX NTT（se [8,256] → ŝ‖ê）
 *   18：AIV MultiplyNTTs 内积 + ê + mod q → t_hat [4,256]
 *   19–20：AIV 嵌 C ByteEncode₁₂(t̂)→ek_polyvec；ByteEncode₁₂(ŝ)→sk_polyvec
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "byte_encode12_aiv.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "hat_debug_config.hpp"
#include "tiling.h"

enum MachineState : uint16_t {
    IDLE = 0,
    AIV_SPLIT,
    AIC_MMAD,
    AIV_MERGE,
};

#ifdef ASCENDC_CPU_DEBUG
volatile int g_alg13_mix_pass = 0;
#endif

__aicore__ inline void alg13_line18_aiv_done_inc(GM_ADDR ws)
{
    __gm__ volatile int32_t *cnt = reinterpret_cast<__gm__ volatile int32_t *>(ws + tiling::LINE18_AIV_SYNC);
    cnt[0] += 1;
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline int32_t alg13_line18_aiv_done_get(GM_ADDR ws)
{
    __gm__ volatile int32_t *cnt = reinterpret_cast<__gm__ volatile int32_t *>(ws + tiling::LINE18_AIV_SYNC);
    return cnt[0];
}

__aicore__ inline void __WAIT(MachineState STATE, const bool AIC, const int32_t subBlockID)
{
    (void)AIC;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(STATE);
    KYBER_PIPE_ALL();
}

__aicore__ inline void __SET(MachineState STATE, const bool AIC, const int32_t subBlockID)
{
    (void)AIC;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(STATE);
    KYBER_PIPE_ALL();
}

#define WAIT __WAIT(STATE, AIC, subBlockID);
#define SET  __SET(STATE, AIC, subBlockID);

extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR dst, GM_ADDR t_hat, GM_ADDR ek_out, GM_ADDR sk_out,
                                                  GM_ADDR src, GM_ADDR a_hat, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    const auto nPoly = static_cast<uint16_t>(tiling.kPolys);

#ifdef ASCENDC_CPU_DEBUG
    const bool runS1 = (g_alg13_mix_pass == 0 || g_alg13_mix_pass == 1);
    const bool runS2 = (g_alg13_mix_pass == 0 || g_alg13_mix_pass == 2);
    const bool runS3 = (g_alg13_mix_pass == 0 || g_alg13_mix_pass == 3);
    const bool runS18 = (g_alg13_mix_pass == 0 || g_alg13_mix_pass == 4);
    const bool runS19 = (g_alg13_mix_pass == 0 || g_alg13_mix_pass == 5);
#else
    const bool runS1 = (tiling.mixPass == 0 || tiling.mixPass == 1);
    const bool runS2 = (tiling.mixPass == 0 || tiling.mixPass == 2);
    const bool runS3 = (tiling.mixPass == 0 || tiling.mixPass == 3);
    const bool runS18 = (tiling.mixPass == 0 || tiling.mixPass == 4);
    const bool runS19 = (tiling.mixPass == 0 || tiling.mixPass == 5);
#endif
    const bool syncS1S2 = runS1 && runS2;
    const bool syncS2S3 = runS2 && runS3;

    MachineState STATE;

    if (AIC) {
        if (!runS2) {
            return;
        }
        if (syncS1S2) {
            STATE = AIV_SPLIT;
            WAIT
        }
        STATE = AIC_MMAD;
        AicMmad mmad(static_cast<uint16_t>(mRows), coeffN, static_cast<uint16_t>(lutHalfCols));
        mmad.Init();
        mmad.Process(ws + MAT_C, ws + S0, ws + LUT_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C + matCHalfBytes, ws + S0, ws + LUT_BOTTOM);
        KYBER_PIPE_ALL();
        if (syncS2S3) {
            SET
        }
    } else {
        if (runS1) {
            STATE = AIV_SPLIT;
            AivSplit split(subBlockID, coeffN, nPoly);
            split.Init(ws + S0, src, ws + S2, ws + S3);
            split.CopyIn();
            split.Compute();
            split.CopyOut();
            KYBER_PIPE_ALL();
            if (syncS1S2) {
                SET
            }
        }

        if (runS3) {
            if (syncS2S3) {
                STATE = AIC_MMAD;
                WAIT
            }

            STATE = AIV_MERGE;
            AivTag5tRouteAMod routeA(subBlockID, coeffN, nPoly);
            routeA.Init(dst, ws + MAT_C, ws + MAT_C + matCHalfBytes);
            routeA.Process();
            KYBER_PIPE_ALL();
        }

        if (runS18) {
            if (runS3) {
                AscendC::PipeBarrier<PIPE_MTE3>();
#if HAT_LINE18_STRONG_SYNC
                AscendC::PipeBarrier<PIPE_MTE2>();
                AscendC::PipeBarrier<PIPE_V>();
#endif
                KYBER_PIPE_ALL();
            }
            AivHatLine18 line18(subBlockID, coeffN);
            line18.Init(t_hat, dst, a_hat);
            line18.Process();
            KYBER_PIPE_ALL();
#ifdef ASCENDC_CPU_DEBUG
            /* CPU 孪生 AIV_0/AIV_1 分进程 launch，进程内 volatile 不共享 */
            alg13_line18_aiv_done_inc(ws);
#endif
        }

        if (runS19) {
            bool doEncode = true;
#ifdef ASCENDC_CPU_DEBUG
            doEncode = (!runS18) || (alg13_line18_aiv_done_get(ws) >= 2);
#endif
#ifndef ASCENDC_CPU_DEBUG
            const int32_t lastAiv = static_cast<int32_t>(AscendC::GetSubBlockNum()) - 1;
            doEncode = (subBlockID == lastAiv);
#endif
#ifdef ASCENDC_CPU_DEBUG
            const bool runEnc = doEncode;
#else
            const int32_t lastAivEnc = static_cast<int32_t>(AscendC::GetSubBlockNum()) - 1;
            const bool runEnc = doEncode && (subBlockID == lastAivEnc);
#endif
            if (runEnc) {
                if (runS18) {
                    AscendC::PipeBarrier<PIPE_MTE3>();
                    AscendC::PipeBarrier<PIPE_MTE2>();
                    AscendC::PipeBarrier<PIPE_V>();
                    AscendC::PipeBarrier<PIPE_ALL>();
                    KYBER_PIPE_ALL();
                }
                AivByteEncode1319 enc(coeffN);
                enc.Init(ek_out, sk_out, t_hat, dst);
                enc.Process();
                KYBER_PIPE_ALL();
            }
        }
    }
}
