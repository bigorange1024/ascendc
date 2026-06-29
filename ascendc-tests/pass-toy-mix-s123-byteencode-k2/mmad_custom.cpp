/**
 * @file mmad_custom.cpp
 * @brief pass-toy-mix-s123-byteencode-k2 的 MIX kernel 入口与 AIC↔AIV CrossCore 编排。
 *
 * 单趟 PEM（KERNEL_TYPE_MIX_AIC_1_2）三阶段：
 *   S1  AivToySplit（双 AIV 并行）     → ws+S0  左矩阵 A[int8, 64×64]
 *   S2  AicMmad(64,64,64)（单 AIC）    → ws+MAT_C  C[int32, 64×64]，B=I 在 ws+LUT
 *   S3  AivToyS3Encode（双 AIV 并行）  → out[int8, 4096]
 *
 * CrossCore 同步（无 AIV↔AIV）：
 *   AIV0/1 完成 S1 后 SET(AIV_SPLIT)
 *   AIC  WAIT(AIV_SPLIT) → matmul → SET(AIC_MMAD)
 *   AIV0/1 WAIT(AIC_MMAD) → S3
 *
 * mixPass 分阶段调试见 tiling.h / TOY_MIX_S123.md。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

/** CrossCore 事件 ID，与 polybatch / planar 探针同一套 SET/WAIT 模式。 */
enum MachineState : uint16_t {
    IDLE = 0,
    AIV_SPLIT, /**< S1 双 AIV 写完 S0 */
    AIC_MMAD,  /**< S2 Cube 写完 MAT_C；S3 入口等待此 flag */
};

#ifdef ASCENDC_CPU_DEBUG
/** CPU 单测无法从 tiling 结构体可靠传 mixPass 时，由 main 写入此全局变量。 */
volatile int g_toy_mix_pass = 0;
#endif

/** 等待对端 SET 的 flag；PIPE_MTE2 与现有 fix-f203 探针保持一致。 */
__aicore__ inline void __WAIT(MachineState STATE, const bool AIC, const int32_t subBlockID)
{
    (void)AIC;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(STATE);
    KYBER_PIPE_ALL();
}

/** 通知对端可继续；与 __WAIT 成对使用。 */
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
 * @param out  GM 输出 out[4096] int8
 * @param src  GM 输入 src[2048] int32
 * @param ws   GM workspace（S0 | LUT | MAT_C，见 tiling.h）
 * @param tiling mixPass 等运行时参数
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR out, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

#ifdef ASCENDC_CPU_DEBUG
    const bool runS1 = (g_toy_mix_pass == 0 || g_toy_mix_pass == 1);
    const bool runS2 = (g_toy_mix_pass == 0 || g_toy_mix_pass == 2);
    const bool runS3 = (g_toy_mix_pass == 0 || g_toy_mix_pass == 3);
#else
    const bool runS1 = (tiling.mixPass == 0 || tiling.mixPass == 1);
    const bool runS2 = (tiling.mixPass == 0 || tiling.mixPass == 2);
    const bool runS3 = (tiling.mixPass == 0 || tiling.mixPass == 3);
#endif
    // 仅当相邻阶段都启用时才发/收 CrossCore，避免单阶段调试时死等。
    const bool syncS1S2 = runS1 && runS2;
    const bool syncS2S3 = runS2 && runS3;

    // MIX：GetSubBlockNum()==1 为 Cube（AIC），否则为 Vector（AIV，idx 0/1）。
    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    MachineState STATE;

    if (AIC) {
        // ======================== AIC 路径：仅 Stage2 ========================
        if (!runS2) {
            return;
        }
        if (syncS1S2) {
            // 全流程 / mixPass=2 且带 preset S0 时跳过 WAIT；有 S1 则等双 AIV 填完左矩阵。
            STATE = AIV_SPLIT;
            WAIT
        }
        STATE = AIC_MMAD;
        // C[64×64] = A[64×64] @ B[64×64]；B=I 时 C 与 A 元素一致（int32 累加）。
        AicMmad mmad(static_cast<uint16_t>(kRows), static_cast<uint16_t>(kDim), static_cast<uint16_t>(kCols));
        mmad.Init();
        mmad.Process(ws + MAT_C, ws + S0, ws + LUT);
        KYBER_PIPE_ALL();
        if (syncS2S3) {
            // 通知双 AIV：MAT_C 已就绪，可进 S3。
            SET
        }
    } else {
        // ======================== AIV 路径：Stage1 + Stage3 ========================
        if (runS1) {
            STATE = AIV_SPLIT;
            AivToySplit split(subBlockID);
            split.Init(ws, src);
            split.Process();
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
        AivToyS3Encode s3(subBlockID);
        s3.Init(out, ws);
        s3.Process();
        KYBER_PIPE_ALL();
    }
}
