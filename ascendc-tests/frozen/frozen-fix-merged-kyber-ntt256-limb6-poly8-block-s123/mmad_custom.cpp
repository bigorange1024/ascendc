/**
 * poly8 block s123：S0/A0/A1 行序 [HI_8 | LO_8]；Stage2 仍 2×Mmad(M0/M1)；LUT 不变。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "debug_print.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

enum MachineState : uint16_t {
    IDLE = 0,
    AIV_SPLIT,
    AIC_MMAD,
    AIV_MERGE,
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
        mmad.Process(ws + A0, ws + S0, ws + M0);
        KYBER_PIPE_ALL();
        mmad.Process(ws + A1, ws + S0, ws + M1);
        KYBER_PIPE_ALL();
        kyber_print_stage2_matrix("A0 after M0", ws + A0, static_cast<uint32_t>(mRows), coeffN);
        kyber_print_stage2_matrix("A1 after M1", ws + A1, static_cast<uint32_t>(mRows), coeffN);
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
            if (subBlockID == 0) {
                kyber_print_s0_int8("S0 after split", ws + S0, static_cast<uint32_t>(mRows), coeffN);
            }
        }
        SET

        STATE = AIC_MMAD;
        WAIT

        STATE = AIV_MERGE;
        {
            AivMerge merge(subBlockID, coeffN, nPoly, 3329);
            merge.Init(dst, ws + A0, ws + A1);
            merge.CopyIn();
            merge.Compute();
            merge.CopyOut();
            KYBER_PIPE_ALL();
        }
    }
}
