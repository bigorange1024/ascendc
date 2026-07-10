/**
 * @file kyber_limb6.hpp
 * @brief Kyber/ML-KEM limb6 编码常量与 PIPE 屏障宏（Stage1 分裂 / Stage3 Horner 移位）。
 *
 * 流水线位置：被 ntt_vec.hpp、stage3_mod_variants.hpp、aiv_func.hpp、mmad_custom.cpp 引用。
 *
 * 作用：
 *   - kKyberLimbBits=6、kKyberLimbMask=0x3f：Stage1 将 int32 系数拆成 hi/lo 各 6-bit；
 *   - kKyberMergeShift{1,2,3}=6/12/18：Stage3 RouteA Horner 合并移位；
 *   - KYBER_PIPE_ALL：向量路径同步 V 管道（CPU debug 下为空操作）。
 *
 * 与 golden 关系：间接经 dst.bin；无独立 limb golden。合并 shift 与平面 mat_c 四 limb 行语义一致。
 *
 * CMake：无；ASCENDC_CPU_DEBUG 时 KYBER_PIPE_ALL 为空操作。
 */
#ifndef KYBER_LIMB6_HPP
#define KYBER_LIMB6_HPP

#include "kernel_operator.h"

/** limb 位宽：6 → 单系数拆成 hi/lo 两路 int8（各 ∈[0,63]） */
constexpr int32_t kKyberLimbBits = 6;
/** 低 6 位掩码，等价于 & 0x3f */
constexpr int32_t kKyberLimbMask = 0x3f;
/** RouteA Horner：每步 ×64 对应左移 6 */
constexpr int32_t kKyberMergeShift1 = 6;
/** 两步合并累计左移 12（×4096 路径中的中间量） */
constexpr int32_t kKyberMergeShift2 = 12;
/** 三步累计左移 18（完整 raw 重建时的高位尺度） */
constexpr int32_t kKyberMergeShift3 = 18;

#ifndef ASCENDC_CPU_DEBUG
/** 设备侧：全管道屏障，保证向量写后读序 */
#define KYBER_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#else
/** CPU 孪生：无硬件管道，宏为空 */
#define KYBER_PIPE_ALL() ((void)0)
#endif

#endif
