/**
 * @file kyber_limb6.hpp
 * @brief limb6 编码/合并常量与 PIPE 宏；NTT/INTT Stage1–3 共用。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-1024 Encrypt 的 NTT(y)/INTT 段。
 * 系数按 6-bit limb 拆成 HI/LO；合并时移位 6/12/18。与 golden：I/O 对拍以 c.bin 为准。
 */
#ifndef KYBER_LIMB6_HPP
#define KYBER_LIMB6_HPP

#include "kernel_operator.h"

constexpr int32_t kKyberLimbBits = 6;       // 每 limb 位数
constexpr int32_t kKyberLimbMask = 0x3f;    // 低 6 位掩码
constexpr int32_t kKyberMergeShift1 = 6;    // RouteA 合并移位
constexpr int32_t kKyberMergeShift2 = 12;
constexpr int32_t kKyberMergeShift3 = 18;

#ifndef ASCENDC_CPU_DEBUG
/** 设备：全管道屏障。 */
#define KYBER_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()
#else
/** CPU 孪生：屏障为空操作（tikicpu 无真实 PIPE）。 */
#define KYBER_PIPE_ALL() ((void)0)
#endif

#endif
