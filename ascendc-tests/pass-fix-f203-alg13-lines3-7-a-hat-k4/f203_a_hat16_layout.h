/**
 * @file f203_a_hat16_layout.h
 * @brief Alg.13 行 3–7：16× SampleNTT 几何与 GM 布局（k=4，与单 poly alg7 几何锁定一致）。
 *
 * 流水线：pass-fix-f203-alg13-lines3-7-a-hat-k4 专用；XOF/d12/rej 常量与
 * pass-fix-f203-alg7-sample-ntt-k4 的 f203_alg7_layout.h 同值。
 *
 * GM 布局与 vec-k4 hat_dot_layout::a_hat_offset(p,j) 一致。
 */
#pragma once

#include "f203_a_hat16_config.h"
#include "f203_alg7_layout.h"

#include <cstdint>

namespace F203Ahat16 {

constexpr uint32_t kKyberK = 4U;
constexpr uint32_t kKyberN = 256U;
constexpr uint32_t kKyberQ = 3329U;

constexpr uint32_t kRhoBytes = 32U;
constexpr uint32_t kSampleSeedBytes = 34U;

constexpr uint32_t kShake128Rate = 168U;
constexpr uint32_t kXofSqueezeBlocks = F203Alg7::kXofSqueezeBlocks;
constexpr uint32_t kXofBytes = F203Alg7::kXofBytes;
constexpr uint32_t kCandPairs = F203Alg7::kCandPairs;
constexpr uint32_t kStreamLen = F203Alg7::kStreamLen;

constexpr uint32_t kAHatPolys = kKyberK * kKyberK;  // 16
constexpr uint32_t kAHatBytes = kAHatPolys * kKyberN * sizeof(int32_t);
constexpr uint32_t kShakeBatch = kAHatPolys;
constexpr uint32_t kXofBatchBytes = kShakeBatch * kXofBytes;

constexpr uint32_t kD12Bytes = kCandPairs * sizeof(int32_t);
constexpr uint32_t kPolyAHatBytes = kKyberN * sizeof(int32_t);

/** 行主序：Â[p,j] 在扁平 a_hat GM 中的起始 int32 下标（Host / golden 用）。 */
constexpr uint32_t AHatOffset(uint32_t p, uint32_t j)
{
    return (p * kKyberK + j) * kKyberN;
}

/** polyIdx = p*K+j 时的 (p,j)。遍历顺序：p 外、j 内。 */
constexpr void PolyIdxToPJ(uint32_t polyIdx, uint8_t &p, uint8_t &j)
{
    p = static_cast<uint8_t>(polyIdx / kKyberK);
    j = static_cast<uint8_t>(polyIdx % kKyberK);
}

}  // namespace F203Ahat16
