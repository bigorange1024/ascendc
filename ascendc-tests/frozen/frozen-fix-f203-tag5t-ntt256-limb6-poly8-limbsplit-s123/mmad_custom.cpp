/**
 * FIPS203 Tag5T batch8 — Stage1+2+3：
 *   S0 [16,256] → 2× Mmad → mat_c [32,256]（C_lo|C_hi）→ Horner+Barrett → dst [8,256]
 */
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
    AIV_MERGE,
};

#ifdef ASCENDC_CPU_DEBUG
volatile int g_tag5t_mix_pass = 0;
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

extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    const auto nPoly = static_cast<uint16_t>(tiling.kPolys);

#ifdef ASCENDC_CPU_DEBUG
    const bool runS1 = (g_tag5t_mix_pass == 0 || g_tag5t_mix_pass == 1);
    const bool runS2 = (g_tag5t_mix_pass == 0 || g_tag5t_mix_pass == 2);
    const bool runS3 = (g_tag5t_mix_pass == 0 || g_tag5t_mix_pass == 3);
#else
    const bool runS1 = (tiling.mixPass == 0 || tiling.mixPass == 1);
    const bool runS2 = (tiling.mixPass == 0 || tiling.mixPass == 2);
    const bool runS3 = (tiling.mixPass == 0 || tiling.mixPass == 3);
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

        if (!runS3) {
            return;
        }

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
}
