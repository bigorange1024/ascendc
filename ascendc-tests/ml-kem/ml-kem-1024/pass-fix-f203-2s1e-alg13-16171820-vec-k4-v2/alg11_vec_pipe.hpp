/**
 * @file alg11_vec_pipe.hpp
 * @brief Alg.11 向量路径的 PIPE / MTE2→V 同步宏（tikicpu 与设备行为差异封装）。
 *
 * 用途：DataCopy 写 UB 后、Gather/向量读 UB 前插入正确屏障；避免 SIM 上 MTE2 与 V 流水线竞态。
 *
 * 调用方：multiply_ntts_vec.hpp、hat_alg11_basemul.hpp、alg11_ub_load 调用链。
 *
 * 不变量：ASCENDC_CPU_DEBUG 用 HardEvent::MTE2_V + WaitFlag；设备用 PipeBarrier<PIPE_MTE2/ALL>。
 *
 * Golden：无；同步错误表现为 SIM golden 不一致或挂死（KERNEL_COMPUTE_BUDGET_SEC 超时）。
 *
 * CMake：RUN_MODE=cpu 时 ASCENDC_CPU_DEBUG 由 cpu_lib.cmake 定义。
 */
#pragma once

#include "kernel_operator.h"

/**
 * DataCopy(MTE2) 写 UB 后、Gather/向量读 UB 前的同步。
 * CPU 孪生：HardEvent::MTE2_V WaitFlag；设备：PipeBarrier<PIPE_ALL>。
 */
__aicore__ inline void alg11_mte2_to_v_sync()
{
#if defined(ASCENDC_CPU_DEBUG)
    constexpr auto evt = AscendC::HardEvent::MTE2_V;
    AscendC::WaitFlag<evt>(0);
#else
    AscendC::PipeBarrier<PIPE_ALL>();
#endif
}

#if defined(ASCENDC_CPU_DEBUG)
#define ALG11_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#define ALG11_PIPE_MTE2() alg11_mte2_to_v_sync()
#else
#define ALG11_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#define ALG11_PIPE_MTE2() AscendC::PipeBarrier<PIPE_MTE2>()
#endif
