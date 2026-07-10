/**
 * @file mmad_custom.cpp
 * @brief 8-poly 紧凑三段式 NTT/INTT 设备入口（MIX 1×AIC + 2×AIV）；LUT 由 host 选择 NTT/INTT。
 *
 * 流水线位置：本文件为唯一 `__global__` kernel；由 main.cpp 经 ICPU_RUN_KF / ACLRT_LAUNCH_KERNEL 启动。
 *
 * 状态机（CrossCore 同步）：
 *   AIV_SPLIT →（AIV 完成 Stage1）→ AIC_MMAD →（AIC 四路 Cube）→ AIV_PACK →（平面 pack）→ Stage3 merge。
 *
 * mixPass：0 仅 S1；1 仅 S2（需 s0_preset）；2 仅 S3（需 mat_c_preset）；3 全链（默认）。
 *
 * 与 golden 关系：dst [8,256] int32 对拍 golden_dst；NTT/INTT 仅差 workspace 内 LUT bin，本 FSM 不变。
 *
 * 语义：poly-batch 双 AIV；平面 mat_c；三段式内无 Gather。
 */
#include "aic_func.hpp"
#include "aiv_func.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"

/** CrossCore 握手状态：IDLE 未用；AIV_SPLIT / AIC_MMAD / AIV_PACK 对应三段边界 */
enum MachineState : uint16_t {
    IDLE = 0,
    AIV_SPLIT,
    AIC_MMAD,
    AIV_PACK,
};

#ifdef ASCENDC_CPU_DEBUG
/** CPU 孪生：mixPass 由 host 写入全局，避免 tiling 结构在 KF 路径传递差异 */
volatile int g_stage123_k8_mix_pass = 3;
#endif

/**
 * 跨核等待：PIPE_ALL 后 CrossCoreWaitFlag(STATE)。
 * @param STATE      当前握手状态枚举
 * @param AIC        是否 AIC 核（本实现未分支使用，保留签名）
 * @param subBlockID 子核号（未使用）
 */
__aicore__ inline void __WAIT(MachineState STATE, const bool AIC, const int32_t subBlockID)
{
    (void)AIC;
    (void)subBlockID;
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<2, PIPE_MTE2>(STATE);
    KYBER_PIPE_ALL();
}

/**
 * 跨核置位：PIPE_ALL 后 CrossCoreSetFlag(STATE)，对端 WAIT 解除。
 */
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
 * MIX kernel 入口。
 *
 * @param dst     输出 GM [8,256] int32
 * @param src     输入 GM [8,256] int32
 * @param ws      workspace：LUT + S0 + 四临时 + 平面 mat_c（见 tiling.h 偏移）
 * @param tiling  tileLength/kPolys/mixPass
 * 前置：host 已装载 LUT（及按 mixPass 的 preset）；blockDim=1，任务类型 MIX_AIC_1_2
 */
extern "C" __global__ __aicore__ void mmad_custom(GM_ADDR dst, GM_ADDR src, GM_ADDR ws, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    using namespace tiling;

    // GetSubBlockNum()==1 表示本核为 AIC；否则为 AIV，subBlockID 为 0/1
    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    const auto coeffN = static_cast<uint32_t>(tiling.tileLength);

#ifdef ASCENDC_CPU_DEBUG
    const int32_t mixPass = g_stage123_k8_mix_pass;
#else
    const int32_t mixPass = tiling.mixPass;
#endif

    // 由 mixPass 推导各段是否执行及是否需要跨核同步
    const bool runS1 = (mixPass == 0 || mixPass == 3);
    const bool runS2 = (mixPass == 1 || mixPass == 3);
    const bool runS3 = (mixPass == 2 || mixPass == 3);
    const bool syncS1S2 = runS1 && runS2;
    const bool syncS2Pack = runS2;

    MachineState STATE;

    if (AIC) {
        // AIC 只参与 Stage2；若仅 S1/S3 则直接返回
        if (!runS2) {
            return;
        }
        // 全链时等待双 AIV 完成 Stage1（S0 就绪）
        if (syncS1S2) {
            STATE = AIV_SPLIT;
            WAIT
        }
        STATE = AIC_MMAD;
        // m=16 行 S0 × k=256 × n=128 半列；四次 Process 对应 even/odd × top/bottom LUT
        AicMmad mmad(static_cast<uint16_t>(mRowsLogic), coeffN, static_cast<uint16_t>(halfN));
        mmad.Init();
        mmad.Process(ws + MAT_C_TMP_LO_EVEN, ws + S0, ws + LUT_EVEN_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_LO_ODD, ws + S0, ws + LUT_ODD_TOP);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_EVEN, ws + S0, ws + LUT_EVEN_BOTTOM);
        KYBER_PIPE_ALL();
        mmad.Process(ws + MAT_C_TMP_HI_ODD, ws + S0, ws + LUT_ODD_BOTTOM);
        KYBER_PIPE_ALL();
        // 通知 AIV 可开始平面 pack
        if (syncS2Pack) {
            STATE = AIV_PACK;
            SET
        }
    } else {
        // ---- AIV：Stage1 →（等 AIC）→ Pack → Stage3 ----
        if (runS1) {
            STATE = AIV_SPLIT;
            AivK8Split split(subBlockID, coeffN);
            split.Init(ws + S0, src);
            split.Process();
            KYBER_PIPE_ALL();
            if (syncS1S2) {
                SET
            }
        }
        if (runS2) {
            STATE = AIV_PACK;
            WAIT
            AivK8PackMatCPlanar pack(subBlockID, coeffN);
            pack.Init(ws + MAT_C_PLANAR, ws + MAT_C_TMP_LO_EVEN, ws + MAT_C_TMP_LO_ODD, ws + MAT_C_TMP_HI_EVEN,
                      ws + MAT_C_TMP_HI_ODD);
            pack.Process();
            KYBER_PIPE_ALL();
        }
        if (runS3) {
            AivK8RouteAMod merge(subBlockID, coeffN);
            merge.Init(dst, ws + MAT_C_PLANAR);
            merge.Process();
            KYBER_PIPE_ALL();
        }
    }
}
