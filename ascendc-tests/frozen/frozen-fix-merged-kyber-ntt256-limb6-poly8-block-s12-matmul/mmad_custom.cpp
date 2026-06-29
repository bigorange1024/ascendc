/**
 * poly8 block s12-matmul：Stage1（block-s123 Split）+ Stage2（手写 MIX Matmul<>，非融合模板）。
 */
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "matmul_stage2.hpp"
#include "tiling.h"

enum MachineState : uint16_t {
    IDLE = 0,
    AIV_SPLIT,
    AIC_MATMUL,
};

#ifdef ASCENDC_CPU_DEBUG
volatile int g_mix_s12_pass = 1;
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

extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR matC, GM_ADDR src, GM_ADDR ws, GM_ADDR mmWorkspace,
                                                  GM_ADDR tilingGm, TilingData tiling)
{
#ifndef ASCENDC_CPU_DEBUG
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
#endif
    (void)mmWorkspace;

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    const auto nPoly = static_cast<uint16_t>(tiling.kPolys);

#ifdef ASCENDC_CPU_DEBUG
    const bool runS1 = (g_mix_s12_pass == 1);
    const bool runS2 = (g_mix_s12_pass == 2);
#else
    const bool runS1 = (tiling.mixPass == 0 || tiling.mixPass == 1);
    const bool runS2 = (tiling.mixPass == 0);
#endif
    const bool needSync = runS1 && runS2;

    MachineState STATE;
    AscendC::TPipe pipe;

    if ASCEND_IS_AIV {
        if (!runS1) {
            return;
        }
    } else if (AIC) {
        if (!runS2) {
            return;
        }
        if (needSync) {
            STATE = AIV_SPLIT;
            WAIT
        }
        STATE = AIC_MATMUL;
        Stage2MatmulCube(ws + ::tiling::S0, ws + ::tiling::B_LUT, matC, mmWorkspace, tilingGm, &pipe);
        KYBER_PIPE_ALL();
    }
    if (!AIC) {
        STATE = AIV_SPLIT;
        AivSplit split(subBlockID, coeffN, nPoly);
        split.Init(ws + ::tiling::S0, src, ws + ::tiling::S2, ws + ::tiling::S3);
        split.CopyIn();
        split.Compute();
        split.CopyOut();
        KYBER_PIPE_ALL();
        if (needSync) {
            SET
            STATE = AIC_MATMUL;
            WAIT
        }
    }
}
