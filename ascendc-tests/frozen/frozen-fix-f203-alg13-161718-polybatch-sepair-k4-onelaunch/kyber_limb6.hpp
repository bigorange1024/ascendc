#ifndef KYBER_LIMB6_HPP
#define KYBER_LIMB6_HPP

#include "kernel_operator.h"

constexpr int32_t kKyberLimbBits = 6;
constexpr int32_t kKyberLimbMask = 0x3f;
constexpr int32_t kKyberMergeShift1 = 6;
constexpr int32_t kKyberMergeShift2 = 12;
constexpr int32_t kKyberMergeShift3 = 18;

#ifndef ASCENDC_CPU_DEBUG
#define KYBER_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#else
#define KYBER_PIPE_ALL() ((void)0)
#endif

#endif
