/**
 * @file f203_kem_dec_layout.h
 * @brief Alg.21 ML-KEM.Decaps I/O 与 dk_kem 切片偏移（ml_kem_1024 / k=4）。
 *
 * 独立常量头：不 include vendor layout，供 decrypt/encrypt 分库共用。
 */
#pragma once

#include <cstdint>

namespace F203KemDec {

constexpr uint32_t kDkKemBytes = 3168U;
constexpr uint32_t kDkPkeOffset = 0U;
constexpr uint32_t kEkOffset = 1536U;
constexpr uint32_t kHOffset = 3104U;
constexpr uint32_t kZOffset = 3136U;

constexpr uint32_t kSharedSecretBytes = 32U;
constexpr uint32_t kHashBytes = 32U;
constexpr uint32_t kGOutBytes = 64U;
constexpr uint32_t kCtBytes = 1568U;
constexpr uint32_t kCoinsBytes = 32U;

}  // namespace F203KemDec
