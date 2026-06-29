/**
 * @file f203_keygen_layout.h
 * @brief Alg.13 KeyGen 行 21 I/O 尺寸（k=4，ML-KEM-768）。
 */
#pragma once

#include <cstdint>

namespace F203Keygen {

constexpr uint32_t kKyberK = 4U;
constexpr uint32_t kPolyBytesEncoded = 384U;
constexpr uint32_t kEkPolyvecBytes = kKyberK * kPolyBytesEncoded;  // 1536
constexpr uint32_t kRhoBytes = 32U;
constexpr uint32_t kEkPkeBytes = kEkPolyvecBytes + kRhoBytes;      // 1568
constexpr uint32_t kDkPkeBytes = kEkPolyvecBytes;                  // 1536

}  // namespace F203Keygen
