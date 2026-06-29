/**
 * Tag5T poly-batch Stage1+2（平面 mat_c）：
 *   S1 AivSplitPolyBatch → S0
 *   S2 4×AicMmad(16,256,128) 偶/奇 LUT × C_lo/C_hi → AivPackMatCPlanar → mat_c [64,128]
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
    AIV_PACK,
};

#ifdef ASCENDC_CPU_DEBUG
volatile int g_planar_mix_pass = 0;
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

extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR mat_c_planar, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    const auto halfCols = static_cast<uint16_t>(halfN);

#ifdef ASCENDC_CPU_DEBUG
    const bool runS1 = (g_planar_mix_pass == 0 || g_planar_mix_pass == 1);
    const bool runS2 = (g_planar_mix_pass == 0 || g_planar_mix_pass == 2);
#else
    const bool runS1 = (tiling.mixPass == 0 || tiling.mixPass == 1);
    const bool runS2 = (tiling.mixPass == 0 || tiling.mixPass == 2);
#endif
    const bool syncS1S2 = runS1 && runS2;

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
        STATE = AIV_PACK;
        SET
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

        if (!runS2) {
            return;
        }

        STATE = AIV_PACK;
        WAIT
        AivPackMatCPlanar pack(subBlockID, coeffN);
        pack.Init(mat_c_planar, ws + MAT_C_TMP_LO_EVEN, ws + MAT_C_TMP_LO_ODD, ws + MAT_C_TMP_HI_EVEN,
                  ws + MAT_C_TMP_HI_ODD);
        pack.Process();
        KYBER_PIPE_ALL();
    }
}
