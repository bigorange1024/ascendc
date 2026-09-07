#ifndef T01_MIX_NTT13_HANDSHAKE_KYBER_LIMB6_HPP
#define T01_MIX_NTT13_HANDSHAKE_KYBER_LIMB6_HPP

/**
 * @file kyber_limb6.hpp
 * @brief 仅提供 KYBER_PIPE_ALL：CrossCore 前后 PIPE_ALL 屏障。
 * 本玩具无真 limb6；文件名沿用活跃 MIX toy 壳约定，便于对照。
 */

#include "kernel_operator.h"

#ifndef ASCENDC_CPU_DEBUG
#define KYBER_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#else
#define KYBER_PIPE_ALL() ((void)0)
#endif

#endif
