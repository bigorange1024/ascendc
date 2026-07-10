/**
 * @file sepolyvec8_ntt_custom.cpp
 * @brief kPolys=8 batch NTT MIX 核（1 AIC + 2 AIV，merged_kyber FSM）。
 *
 * 流水线位置：
 *   AIV Split（limb6）→ AIC MMAD×2（A0/A1）→ AIV Merge（RouteA+Barrett）→ dst[8,256]
 * 输入：src se_polyvec [8,256] int32 + ws 内 LUT；输出 dst NTT 系数。
 * 与 golden：output.bin 与 gen_data golden 对拍；仅 I/O 等价。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

/** 跨核握手状态：AIV_SPLIT → AIC_MMAD → AIV_MERGE。 */
enum MachineState : uint16_t {
    IDLE = 0,
    AIV_SPLIT,
    AIC_MMAD,
    AIV_MERGE,
};

/** CrossCore WaitFlag：等待对端到达 STATE。 */
__aicore__ inline void __WAIT(MachineState STATE, const bool AIC, const int32_t subBlockID)
{
    (void)AIC;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(STATE);
    KYBER_PIPE_ALL();
}

/** CrossCore SetFlag：通知对端本端已完成 STATE。 */
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

/**
 * MIX 核入口。
 * @param dst 输出 [8,256] int32；@param src 输入 se；@param ws workspace（含 LUT 与中间）
 * @param tiling tileLength/kPolys
 */
extern "C" __global__ __aicore__ void sepolyvec8_ntt_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);
    const auto nPoly = static_cast<uint16_t>(tiling.kPolys);

    MachineState STATE;

    // AIC：等 Split 完成后做两次 MMAD（even/odd 或 A0/A1 路径）
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
        // AIV：Stage1 Split → 等 MMAD → Stage3 Merge
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
