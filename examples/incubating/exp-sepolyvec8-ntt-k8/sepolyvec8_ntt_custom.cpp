/**
 * exp-mlkem-sepolyvec8-ntt-k4：kPolys=8 batch NTT（merged_kyber FSM）。
 * 8 条互异随机 poly → dst [8,256] int32；架构对齐 poly8-s123 探针。
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

extern "C" __global__ __aicore__ void sepolyvec8_ntt_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling)
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
