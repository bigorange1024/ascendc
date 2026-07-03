/**
 * @file f203_kem_kg_layout.h
 * @brief Alg.19 KEM KeyGen I/O 尺寸（ml_kem_1024 / k=4；liboqs 展开 dk 布局）。
 */
#pragma once

#include <cstdint>

namespace F203KemKg {

constexpr uint32_t kSeedBytes = 4U;
// 旁路 A（KEM_KG_EXT_SEED，test-only）：host 直接提供的 kem_seed 长度 = d(32)‖z(32)。
// 生产路径（宏关）仍只吃 4B seed_d，由 device UB 派生 d/z；本常量仅供测试构建扩 seed GM 缓冲。
constexpr uint32_t kExtSeedBytes = 64U;
constexpr uint32_t kDkPkeBytes = 1536U;
constexpr uint32_t kEkKemBytes = 1568U;
constexpr uint32_t kHashEkBytes = 32U;
constexpr uint32_t kZBytes = 32U;
/** liboqs：dk_pke ‖ ek ‖ H(ek) ‖ z */
constexpr uint32_t kDkKemBytes = kDkPkeBytes + kEkKemBytes + kHashEkBytes + kZBytes;  // 3168

}  // namespace F203KemKg
