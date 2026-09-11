/**
 * EN03-encrypt-host-skel · AIV 轻桩共用脚手架
 *
 * 背景：Encrypt 形 Host 多段中，Prep/Matvec/Pack 须为轻量 AIV 外形（KB A2），
 *       禁止 CrossCore / GATE 4/8 / SoftSync（B1–B3）。
 * 结论：CPU 声明 KERNEL_TYPE_AIV_ONLY；SIM/NPU 用 MIX_AIC_1_2 占位（AIC 立即 return），
 *       规避「MIX 后再 launch 纯 AIV」的 binary 注册失效（见 qa/2026-06-30 funckey）。
 * 未采用：把桩融进 NTT MIX 胖核；照抄 Encrypt/alg14/ER 核。
 */
#ifndef ENC_AIV_STUB_COMMON_HPP
#define ENC_AIV_STUB_COMMON_HPP

#include "kernel_operator.h"

/**
 * 桩核任务类型：CPU=AIV_ONLY；SIM=MIX 占位且 AIC 空跑无握手。
 * @return true 表示当前为 AIC（仅 SIM MIX 占位路径），调用方应立即 return。
 */
__aicore__ inline bool EncStubSkipIfAicPlaceholder()
{
#if defined(ASCENDC_CPU_DEBUG)
    // CPU：纯 AIV，无 AIC 占位
    return false;
#else
    // SIM：GetSubBlockNum()==1 即 AIC；立即退出，禁止 Wait/Set/SyncAll
    return AscendC::GetSubBlockNum() == 1;
#endif
}

/**
 * 声明桩核 KERNEL_TASK_TYPE：与 EncStubSkipIfAicPlaceholder 配对使用。
 */
#if defined(ASCENDC_CPU_DEBUG)
#define ENC_STUB_KERNEL_TASK_TYPE() \
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY)
#else
#define ENC_STUB_KERNEL_TASK_TYPE() \
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2)
#endif

/**
 * 仅 AIV0（或 AIV_ONLY 单核）执行体量；MIX 占位下 AIV1 直接返回，避免双写。
 */
__aicore__ inline bool EncStubIsWorkerAiv()
{
#if defined(ASCENDC_CPU_DEBUG)
    return true;
#else
    return AscendC::GetSubBlockIdx() == 0;
#endif
}

#endif // ENC_AIV_STUB_COMMON_HPP
