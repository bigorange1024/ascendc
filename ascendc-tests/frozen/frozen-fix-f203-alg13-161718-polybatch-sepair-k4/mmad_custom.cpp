/**
 * Alg.13 行 16–20（k=4，se_pair + poly-batch）：
 *   S3→行18→ByteEncode 融合 UB 路径（无 ŝ/t̂/ê 中间 GM 往返）；mixPass 5 仍单独 S3 dump。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "alg13_post_ntt_ub.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

enum MachineState : uint16_t {
    IDLE = 0,
    AIV_SPLIT,
    AIC_MMAD,
    AIV_PACK,
    AIV_MERGE,
};

#ifdef ASCENDC_CPU_DEBUG
volatile int g_alg13_mix_pass = 0;
#endif

__aicore__ inline void alg13_ub_sync_clear(GM_ADDR ws)
{
    __gm__ volatile int32_t *sync = reinterpret_cast<__gm__ volatile int32_t *>(ws + tiling::ALG13_UB_SYNC);
    sync[0] = 0;
    sync[1] = 0;
    AscendC::PipeBarrier<PIPE_ALL>();
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
    const auto halfCols = static_cast<uint16_t>(halfN);

#ifdef ASCENDC_CPU_DEBUG
    const int32_t mixPass = g_alg13_mix_pass;
#else
    const int32_t mixPass = tiling.mixPass;
#endif
    const bool runS1 = (mixPass == 0 || mixPass == 1 || mixPass == 5 || mixPass == 6);
    const bool runS2 = (mixPass == 0 || mixPass == 2 || mixPass == 5 || mixPass == 6);
    const bool runS3Only = (mixPass == 5);
    const bool runS3 = (mixPass == 0 || mixPass == 3 || mixPass == 5);
    const bool runHat = (mixPass == 0 || mixPass == 4);
    const bool runEncode = (mixPass == 0 || mixPass == 4 || mixPass == 7);
    const bool ubFused = runHat || (runEncode && mixPass == 7);
    const bool loadNttPreset = (mixPass == 4 || mixPass == 7);
    const bool loadThatPreset = (mixPass == 7);
    const bool syncS1S2 = runS1 && runS2;
    const bool syncS2Pack = runS2;

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
        AicMmad mmad(static_cast<uint16_t>(mRows), coeffN, halfCols);
        mmad.Init();
        mmad.Process(ws + MAT_C_TMP_LO_EVEN, ws + S0, ws + LUT_EVEN_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_LO_ODD, ws + S0, ws + LUT_ODD_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_EVEN, ws + S0, ws + LUT_EVEN_BOTTOM);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_ODD, ws + S0, ws + LUT_ODD_BOTTOM);
        KYBER_PIPE_ALL();
        if (syncS2Pack) {
            STATE = AIV_PACK;
            SET
        }
    } else {
        if (runS1) {
            STATE = AIV_SPLIT;
            AivSplitPolyBatch split(subBlockID, coeffN);
            split.Init(ws + S0, src);
            split.Process();
            KYBER_PIPE_ALL();
            if (syncS1S2) {
                SET
            }
        }

        if (runS2) {
            STATE = AIV_PACK;
            WAIT
            AivPackMatCPlanar pack(subBlockID, coeffN);
            pack.Init(ws + MAT_C_PLANAR, ws + MAT_C_TMP_LO_EVEN, ws + MAT_C_TMP_LO_ODD, ws + MAT_C_TMP_HI_EVEN,
                      ws + MAT_C_TMP_HI_ODD);
            pack.Process();
            KYBER_PIPE_ALL();
        }

        if (ubFused) {
            if ((runS3 && !runS3Only) || runHat) {
                alg13_ub_sync_clear(ws);
            }
            AivAlg13UbPipeline pipe(subBlockID, coeffN);
            pipe.Init(ws + MAT_C_PLANAR, ws + SHAT_PEER, ws + ALG13_UB_SYNC, a_hat, ek_out, sk_out, dst, t_hat);
            pipe.Process(runS3 && !loadNttPreset, runHat, runEncode, loadNttPreset, loadThatPreset);
            KYBER_PIPE_ALL();
        } else if (runS3) {
            STATE = AIV_MERGE;
            AivTag5tRouteAModPolyBatch route(subBlockID, coeffN);
            route.Init(dst, ws + MAT_C_PLANAR);
            route.Process();
            KYBER_PIPE_ALL();
        }
    }
}
