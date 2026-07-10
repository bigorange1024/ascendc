/**
 * @file alg11_vec_pipe.hpp
 * @brief Alg.11 向量路径 PIPE 屏障宏：SIM/NPU 有效，CPU debug 为空操作。
 *
 * 流水线位置：multiply_ntts_vec / su_dot 同步点。
 * 背景：ASCENDC_CPU_DEBUG 下 CrossCore/PIPE 语义不同，屏障会干扰孪生；
 * 故 CPU 路径把 ALG11_PIPE_* 编成 no-op。
 */
#pragma once

#include "kernel_operator.h"

#ifndef ASCENDC_CPU_DEBUG
#define ALG11_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#define ALG11_PIPE_MTE2() AscendC::PipeBarrier<PIPE_MTE2>()
#else
#define ALG11_PIPE_ALL() ((void)0)
#define ALG11_PIPE_MTE2() ((void)0)
#endif
