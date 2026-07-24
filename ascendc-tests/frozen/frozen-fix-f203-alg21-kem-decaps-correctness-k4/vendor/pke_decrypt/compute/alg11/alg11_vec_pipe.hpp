#pragma once

#include "kernel_operator.h"

#ifndef ASCENDC_CPU_DEBUG
#define ALG11_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#define ALG11_PIPE_MTE2() AscendC::PipeBarrier<PIPE_MTE2>()
#else
#define ALG11_PIPE_ALL() ((void)0)
#define ALG11_PIPE_MTE2() ((void)0)
#endif
