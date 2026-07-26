#ifndef KYBER_LIMB6_HPP
#define KYBER_LIMB6_HPP

/**
 * @file kyber_limb6.hpp
 * @brief Kyber/ML-KEM 6-bit limb 常量与 PIPE_ALL 宏（本探针主要用于同步屏障）。
 *
 * 流水线位置：设备侧公共常量头；ByteEncode₁₂ 路径主要使用 KYBER_PIPE_ALL。
 * 与 golden 关系：无直接数值影响；屏障保证搬运/向量序，使输出可对拍。
 * 作用：定义 limb 位宽/掩码/合并移位，以及 CPU debug 下空操作的 PipeBarrier 包装。
 */

#include "kernel_operator.h"

/** 单 limb 位宽（历史 limb 编码用；本探针编码路径不直接用） */
constexpr int32_t kKyberLimbBits = 6;
constexpr int32_t kKyberLimbMask = 0x3f;
constexpr int32_t kKyberMergeShift1 = 6;
constexpr int32_t kKyberMergeShift2 = 12;
constexpr int32_t kKyberMergeShift3 = 18;

/** 设备：全管道屏障；CPU 孪生：空操作（避免仿真无 PIPE 语义） */
#ifndef ASCENDC_CPU_DEBUG
#define KYBER_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#else
#define KYBER_PIPE_ALL() ((void)0)
#endif

#endif
