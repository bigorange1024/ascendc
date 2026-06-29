/**
 * FSM shell only（调试 sim CrossCore）：参数布局对齐 mmad_custom（3×GM + TilingData）。
 */
#include "kernel_operator.h"
#include "phase_a_tiling.h"

enum MachineState : uint16_t {
    IDLE = 0,
    AIV_ENCODE,
    AIC_OBSERVE,
};

__aicore__ inline void __WAIT(MachineState STATE, const bool AIC, const int32_t subBlockID)
{
    (void)AIC;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(STATE);
}

__aicore__ inline void __SET(MachineState STATE, const bool AIC, const int32_t subBlockID)
{
    (void)AIC;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<2, PIPE_MTE2>(STATE);
}

#define WAIT __WAIT(STATE, AIC, subBlockID);
#define SET  __SET(STATE, AIC, subBlockID);

extern "C" __global__ __aicore__ void f203_phase_a_fsm_custom(GM_ADDR seGm, GM_ADDR matAGm, GM_ADDR wsGm,
                                                              TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (AscendC::GetBlockIdx() >= AscendC::GetBlockNum()) {
        return;
    }

    (void)seGm;
    (void)matAGm;
    (void)wsGm;
    (void)tiling;

    MachineState STATE;
    if (AIC) {
        STATE = AIV_ENCODE;
        WAIT

        STATE = AIC_OBSERVE;
        SET
    } else {
        STATE = AIV_ENCODE;
        {
            (void)subBlockID;
            SET
        }
    }
}
