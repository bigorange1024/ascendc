/**
 * F203 Stage1+2 MIX：merged_kyber FSM 壳 + Stage1 AIV encode + Stage2 AIC Matmul
 */
#include "f203_aiv_encode.hpp"
#include "f203_stage2_matmul.hpp"
#include "f203_tiling.h"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"

enum MachineState : uint16_t {
    IDLE = 0,
    AIV_ENCODE,
    AIC_MMAD,
};

#ifdef ASCENDC_CPU_DEBUG
volatile int g_f203_host_pass = 1;
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

extern "C" __global__ __aicore__ void f203_stage12_mix_custom(GM_ADDR matCGm, GM_ADDR seGm, GM_ADDR ws,
                                                               GM_ADDR workspaceGm, GM_ADDR tilingGm)
{
#ifndef ASCENDC_CPU_DEBUG
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
#endif

#ifdef ASCENDC_CPU_DEBUG
    const bool runStage1 = (g_f203_host_pass == 1);
    const bool runStage2 = (g_f203_host_pass == 2);
#else
    const bool runStage1 = true;
    const bool runStage2 = true;
#endif
    const bool needSync = runStage1 && runStage2;

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    MachineState STATE;
    AscendC::TPipe pipe;

    if (AIC) {
        if (!runStage2) {
            return;
        }
        if (needSync) {
            STATE = AIV_ENCODE;
            WAIT
        }
        STATE = AIC_MMAD;
        f203_stage2::RunMatmul(ws + f203_ws::MAT_A, ws + f203_ws::LUT, matCGm, workspaceGm, tilingGm, &pipe);
        KYBER_PIPE_ALL();
        if (needSync) {
            SET
        }
    } else {
        if (!runStage1) {
            return;
        }
        STATE = AIV_ENCODE;
        f203_encode::RunEncodeRange(seGm, ws + f203_ws::MAT_A, subBlockID);
        KYBER_PIPE_ALL();
        if (needSync) {
            SET
            STATE = AIC_MMAD;
            WAIT
        }
    }
}
