/**
 * 【文件头】向量路径 PIPE 屏障宏（CPU 孪生下为空操作）。
 *
 * 本文件在流水线中的位置：multiply_ntts_vec.hpp 在 MTE2/向量指令边界插入同步。
 * 作用：非 ASCENDC_CPU_DEBUG 时展开 PipeBarrier；CPU 调试路径跳过以免误报。
 * 与 golden 关系：仅同步语义，不改变计算结果。
 */
#pragma once

#include "kernel_operator.h"

#ifndef ASCENDC_CPU_DEBUG
/** 全管道屏障：向量计算与后续读写之间 */
#define ALG11_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
/** MTE2 屏障：DataCopy/Gather 源就绪后再读 */
#define ALG11_PIPE_MTE2() AscendC::PipeBarrier<PIPE_MTE2>()
#else
#define ALG11_PIPE_ALL() ((void)0)
#define ALG11_PIPE_MTE2() ((void)0)
#endif
