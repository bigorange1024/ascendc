#ifndef ER04_CUBE_NTT_VOLUME_KYBER_LIMB6_HPP
#define ER04_CUBE_NTT_VOLUME_KYBER_LIMB6_HPP

/**
 * @file kyber_limb6.hpp
 * @brief 仅提供 KYBER_PIPE_ALL：CrossCore 前后 PIPE_ALL 屏障。
 * 本骨架无真 limb6；文件名沿用 MIX toy 壳约定，便于对照 T03/T06 脚手架。
 */

#include "kernel_operator.h"

#ifndef ASCENDC_CPU_DEBUG
#define KYBER_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#else
#define KYBER_PIPE_ALL() ((void)0)
#endif

#endif
