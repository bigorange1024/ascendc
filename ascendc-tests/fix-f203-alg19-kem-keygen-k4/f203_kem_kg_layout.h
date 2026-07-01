/**
 * @file f203_kem_kg_layout.h
 * @brief Alg.19 KEM KeyGen I/O 尺寸（ml_kem_1024 / k=4；liboqs 展开 dk 布局）。
 */
#pragma once

#include <cstdint>

namespace F203KemKg {

constexpr uint32_t kSeedBytes = 4U;
constexpr uint32_t kDkPkeBytes = 1536U;
constexpr uint32_t kEkKemBytes = 1568U;
constexpr uint32_t kHashEkBytes = 32U;
constexpr uint32_t kZBytes = 32U;
/** liboqs：dk_pke ‖ ek ‖ H(ek) ‖ z */
constexpr uint32_t kDkKemBytes = kDkPkeBytes + kEkKemBytes + kHashEkBytes + kZBytes;  // 3168

}  // namespace F203KemKg
