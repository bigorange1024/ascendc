/**
 * Phase D @ 6bit limb：Split → 2×Mmad → Merge（架构同 merged_kyber 7bit）
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
    AIV_MERGE
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
    const auto n = tiling.tileLength;

    MachineState STATE;

    if (AIC) {
        STATE = AIV_SPLIT;
        AicMmad mmad(2, n, n);
        mmad.Init();
        WAIT

        STATE = AIC_MMAD;
        mmad.Process(ws + A0, ws + S0, ws + M0);
        KYBER_PIPE_ALL();
        mmad.Process(ws + A1, ws + S0, ws + M1);
        KYBER_PIPE_ALL();
        kyber_print_stage2_matrix("limb6 A0 after M0", ws + A0, 2U, static_cast<uint32_t>(n));
        kyber_print_stage2_matrix("limb6 A1 after M1", ws + A1, 2U, static_cast<uint32_t>(n));
        SET
    } else {
        STATE = AIV_SPLIT;
        {
            AivSplit split(subBlockID, n / 2);
            size_t src_offset = subBlockID * (n / 2) * sizeof(int32_t);
            size_t dst_offset = subBlockID * (n / 2) * sizeof(int8_t);

            split.Init(ws + S0 + dst_offset, ws + S1 + dst_offset, ws + S2 + dst_offset, ws + S3 + dst_offset,
                       src + src_offset);
            split.CopyIn();
            split.Compute();
            split.CopyOut();
            KYBER_PIPE_ALL();
            SET
        }

        STATE = AIC_MMAD;
        WAIT

        STATE = AIV_MERGE;
        {
            AivMerge merge(subBlockID, n, 3329);
            merge.Init(dst + subBlockID * n / 2 * sizeof(int32_t), ws + A0, ws + A1, ws + A2, ws + A3);
            merge.CopyIn();
            merge.Compute();
            merge.CopyOut();
            KYBER_PIPE_ALL();
        }
    }
}
