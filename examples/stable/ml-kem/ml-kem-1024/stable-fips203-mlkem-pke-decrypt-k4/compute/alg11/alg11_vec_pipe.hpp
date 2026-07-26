/**
 * @file alg11_vec_pipe.hpp
 * @brief Alg.11 向量路径 PIPE 屏障宏（Decrypt su_dot / MultiplyNTTs）。
 *
 * 设备侧：ALG11_PIPE_ALL / ALG11_PIPE_MTE2 → AscendC::PipeBarrier。
 * CPU 孪生：空操作，避免调试路径插入无效屏障。
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
