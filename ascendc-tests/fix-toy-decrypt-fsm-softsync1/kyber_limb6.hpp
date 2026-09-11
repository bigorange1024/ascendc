#ifndef FIX_TOY_DECRYPT_FSM_SOFTSYNC1_KYBER_LIMB6_HPP
#define FIX_TOY_DECRYPT_FSM_SOFTSYNC1_KYBER_LIMB6_HPP

/**
 * @file kyber_limb6.hpp
 * @brief 仅提供 KYBER_PIPE_ALL：PIPE_ALL 屏障宏。
 * 本玩具无真 limb6；文件名沿用活跃 MIX toy 壳约定，便于对照。
 */

#include "kernel_operator.h"

#ifndef ASCENDC_CPU_DEBUG
#define KYBER_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#else
#define KYBER_PIPE_ALL() ((void)0)
#endif

#endif
