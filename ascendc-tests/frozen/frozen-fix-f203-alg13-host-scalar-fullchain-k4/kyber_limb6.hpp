/**
 * @file kyber_limb6.hpp
 * @brief Kyber/ML-KEM limb6 编码常量与 PIPE 屏障宏（Stage1 分裂 / Stage3 Horner 移位）。
 *
 * 用途：kKyberLimbBits=6、kKyberMergeShift{1,2,3}；KYBER_PIPE_ALL 在向量 basemul 遗留路径中同步 V 管道。
 *
 * 调用方：ntt_vec.hpp、mod_variants.hpp、hat_vec.hpp、aiv_func.hpp。
 *
 * 不变量：mask=0x3f；合并 shift 6/12/18 与平面 mat_c 四 limb 行语义一致。
 *
 * Golden：间接经 dst.bin；无独立 limb golden。
 *
 * CMake：无；ASCENDC_CPU_DEBUG 时 KYBER_PIPE_ALL 为空操作。
 */
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
