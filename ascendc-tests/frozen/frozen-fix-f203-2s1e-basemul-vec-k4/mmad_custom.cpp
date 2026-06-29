/**
 * mmad_custom — 2s1e MIX kernel（1×AIC + 2×AIV）。
 *
 * 流水线：Stage1(S0) → Stage2(AIC MMAD×2) → AivPack(平面 mat_c)
 *         → Aiv2s1eUbPipeline(S3 merge/mod → 行18 basemul+mod → ByteEncode)。
 *
 * mixPass（tiling / g_2s1e_mix_pass）：0=全链路；1..7=分阶段调试，见 IMPLEMENTATION_REFERENCE.md。
 * host：1×s+1e；设备复制 ŝ，无 SHAT_PEER / AIV↔AIV GM 交换。
 * mat_c：平面 [96,128]，禁止 Gather。
 */
#include "2s1e_post_ntt_ub.hpp"
#include "aic_func.hpp"
#include "aiv_func.hpp"
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
volatile int g_2s1e_mix_pass = 0;
#endif

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
    const int32_t mixPass = g_2s1e_mix_pass;
#else
    const int32_t mixPass = tiling.mixPass;
#endif
    const bool runS1 = (mixPass == 0 || mixPass == 1 || mixPass == 5);
    const bool runS2 = (mixPass == 0 || mixPass == 2 || mixPass == 5);
    const bool runS3Only = (mixPass == 5);
    const bool runS3 = (mixPass == 0 || mixPass == 3 || mixPass == 5);
    const bool runHat = (mixPass == 0 || mixPass == 4);
    const bool runEncode = (mixPass == 0 || mixPass == 4 || mixPass == 7);
    const bool ubFused = runHat || (runEncode && mixPass == 7) || (runS3 && !runS3Only);
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
            Aiv2s1eSplit split(subBlockID, coeffN);
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
            Aiv2s1ePackMatCPlanar pack(subBlockID, coeffN);
            pack.Init(ws + MAT_C_PLANAR, ws + MAT_C_TMP_LO_EVEN, ws + MAT_C_TMP_LO_ODD, ws + MAT_C_TMP_HI_EVEN,
                      ws + MAT_C_TMP_HI_ODD);
            pack.Process();
            KYBER_PIPE_ALL();
        }

        if (ubFused || runS3) {
            Aiv2s1eUbPipeline pipe(subBlockID, coeffN);
            pipe.Init(ws + MAT_C_PLANAR, a_hat, ek_out, sk_out, dst, t_hat);
            pipe.Process(runS3 && !loadNttPreset, runHat, runEncode, loadNttPreset, loadThatPreset);
            KYBER_PIPE_ALL();
        }
    }
}
