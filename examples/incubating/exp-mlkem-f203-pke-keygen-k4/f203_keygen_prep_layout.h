/**
 * @file f203_keygen_prep_layout.h
 * @brief KeyGen 准备段单内核 I/O 尺寸（行 3–15：Â + presample V3）。
 */
#pragma once

#include <cstdint>

#ifndef F203_AHAT16_BLOCK_DIM
#define F203_AHAT16_BLOCK_DIM 2
#endif

namespace F203KeygenPrep {

constexpr uint32_t kKyberK = 4U;
constexpr uint32_t kKyberN = 256U;
constexpr uint32_t kAHatPolys = kKyberK * kKyberK;
constexpr uint32_t kSrcRows = 8U;

constexpr uint32_t kSeBatch = 8U;
constexpr uint32_t kSeMaxMsgLen = 64U;
constexpr uint32_t kSePrfOutLen = 128U;

constexpr size_t kSeedBytes = sizeof(uint32_t);
constexpr size_t kRhoBytes = 32U;
constexpr size_t kAHatBytes = static_cast<size_t>(kAHatPolys) * kKyberN * sizeof(int32_t);
constexpr size_t kSrcBytes = static_cast<size_t>(kSrcRows) * kKyberN * sizeof(int32_t);
constexpr size_t kPrfBytes = static_cast<size_t>(kSeBatch) * kSePrfOutLen;
constexpr size_t kSeXBytes = static_cast<size_t>(kSeBatch) * kSeMaxMsgLen;
constexpr size_t kSeLenBytes = static_cast<size_t>(kSeBatch) * sizeof(uint32_t);
constexpr size_t kSeWsBytes = 64U;

/** 逻辑 AIV 分片：1=单 AIV 16 poly；2=双 AIV 8+8（Opt-4 默认）。 */
constexpr uint32_t kPrepAivShardCount = static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM);
/** Host launch blockDim：恒 1（单 AI Core）；双 AIV 用 GetSubBlockIdx，勿与 shard 数混用。 */
constexpr uint32_t kPrepHostLaunchBlockDim = 1U;
/** 兼容旧名：指 AIV 分片数，非 host blockDim。 */
constexpr uint32_t kPrepBlockDim = kPrepAivShardCount;

}  // namespace F203KeygenPrep
