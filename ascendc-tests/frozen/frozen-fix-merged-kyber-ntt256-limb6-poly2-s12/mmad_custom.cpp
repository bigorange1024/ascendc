/**
 * Phase D′ poly2（fix）：单 TPipe batch Split + Stage2 Mmad。
 * 2×poly → A[4,256] → AicMmad(4,256,256)×2；dst 为 M0 结果 [4,256] int32。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "stage2_debug_print.hpp"
#include "tiling.h"

enum MachineState : uint16_t {
    IDLE = 0,
    AIV_SPLIT,
    AIC_MMAD,
};

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

    MachineState STATE;

    if (AIC) {
        STATE = AIV_SPLIT;
        AicMmad mmad(mRows, coeffN, coeffN);
        mmad.Init();
        WAIT

        STATE = AIC_MMAD;
        mmad.Process(dst, ws + S0, ws + M0);
        KYBER_PIPE_ALL();
        mmad.Process(ws + A1, ws + S0, ws + M1);
        KYBER_PIPE_ALL();
        kyber_print_stage2_matrix("poly2 dst after M0", dst, mRows, coeffN);
        SET
    } else {
        STATE = AIV_SPLIT;
        {
            AivSplit split(subBlockID, coeffN, nPoly);
            split.Init(ws + S0, src, ws + S2, ws + S3);
            split.CopyIn();
            split.Compute();
            split.CopyOut();
            KYBER_PIPE_ALL();
        }
        SET

        STATE = AIC_MMAD;
        WAIT
    }
}
