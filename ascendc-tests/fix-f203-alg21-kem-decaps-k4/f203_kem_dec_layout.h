/**
 * @file f203_kem_dec_layout.h
 * @brief FIPS 203 Alg.21（ML-KEM.Decaps）I/O 与 dk_kem 切片偏移：ml_kem_1024 / k=4。
 *
 * dk_kem 布局与 Alg.19 输出一致（liboqs 展开）：
 *   [0:1536) dk_pke | [1536:3104) ek | [3104:3136) H(ek) | [3136:3168) z
 *
 * 独立常量头：不 include vendor layout，供 Decrypt/Encrypt 分库与本仓 kem/ FO 共用。
 * 生产 I/O：input/dk_kem.bin + c.bin → output/K.bin(32)。
 */
#pragma once

#include <cstdint>

namespace F203KemDec {

constexpr uint32_t kDkKemBytes = 3168U;
/** dk_pke 起始偏移。 */
constexpr uint32_t kDkPkeOffset = 0U;
/** 嵌入的 ek 起始（= kDkPkeBytes）。 */
constexpr uint32_t kEkOffset = 1536U;
/** H(ek) 起始。 */
constexpr uint32_t kHOffset = 3104U;
/** 隐式拒绝秘密 z 起始。 */
constexpr uint32_t kZOffset = 3136U;

constexpr uint32_t kSharedSecretBytes = 32U;
constexpr uint32_t kHashBytes = 32U;
/** G(m'‖h) = SHA3-512 → 64B = K'‖r'。 */
constexpr uint32_t kGOutBytes = 64U;
constexpr uint32_t kCtBytes = 1568U;
constexpr uint32_t kCoinsBytes = 32U;

}  // namespace F203KemDec
