// @probe stable-fips203-mlkem-pke-keygen-k4
// @file compute/alg11_vec_pipe.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `alg11_vec_pipe.hpp` 为该子模块组件。 / Component: alg11_vec_pipe.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


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

/** DataCopy(MTE2) 写 UB 后、Gather/向量读 UB 前；tikicpu 须显式 MTE2_V 配对。 */
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
