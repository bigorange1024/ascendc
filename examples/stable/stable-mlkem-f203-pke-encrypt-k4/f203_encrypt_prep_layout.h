/**
 * @file f203_encrypt_prep_layout.h
 * @brief Alg.14 Encrypt prep 单内核 I/O 尺寸（行 3–15：Â + r/e₁/e₂）。
 */
#pragma once

#include <cstdint>

#ifndef F203_AHAT16_BLOCK_DIM
#define F203_AHAT16_BLOCK_DIM 2
#endif

namespace F203EncryptPrep {

constexpr uint32_t kKyberK = 4U;
constexpr uint32_t kKyberN = 256U;
constexpr uint32_t kAHatPolys = kKyberK * kKyberK;
constexpr uint32_t kEkPkeBytes = 1568U;
constexpr uint32_t kRhoOffset = 1536U;
constexpr uint32_t kRhoBytes = 32U;
constexpr uint32_t kCoinsBytes = 32U;

constexpr uint32_t kEta = 2U;
constexpr uint32_t kRePolys = 2U * kKyberK + 1U;  // 9
constexpr uint32_t kPrfBytesPerPoly = (kEta * kKyberN) / 4U;  // 128
constexpr uint32_t kPrfBatch = kRePolys;
constexpr uint32_t kPrfMsgLen = 33U;  // coins[32] || byte(nonce)

constexpr size_t kEkBytes = static_cast<size_t>(kEkPkeBytes);
constexpr size_t kCoinsSize = static_cast<size_t>(kCoinsBytes);
constexpr size_t kAHatBytes = static_cast<size_t>(kAHatPolys) * kKyberN * sizeof(int32_t);
constexpr size_t kPrfBytes = static_cast<size_t>(kPrfBatch) * kPrfBytesPerPoly;
constexpr size_t kReBytes = static_cast<size_t>(kRePolys) * kKyberN * sizeof(int32_t);

constexpr uint32_t kPrepBlockDim = static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM);

}  // namespace F203EncryptPrep
