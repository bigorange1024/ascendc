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
