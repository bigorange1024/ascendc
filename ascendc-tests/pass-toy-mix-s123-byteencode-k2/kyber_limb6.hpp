#ifndef KYBER_LIMB6_HPP
#define KYBER_LIMB6_HPP

/**
 * @file kyber_limb6.hpp
 * @brief 本探针仅复用 KYBER_PIPE_ALL 宏；不含真 limb6 拆 limb 逻辑（见 aiv_func.hpp 内玩具 limb）。
 *
 * KYBER_PIPE_ALL：在 device 侧于 CrossCore 前后插入 PIPE_ALL barrier；
 * CPU debug 下为空操作，避免 tikicpu 与真机 barrier 行为差异拖慢单测。
 */

#include "kernel_operator.h"

#ifndef ASCENDC_CPU_DEBUG
#define KYBER_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#else
#define KYBER_PIPE_ALL() ((void)0)
#endif

#endif
