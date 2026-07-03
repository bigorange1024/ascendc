/**
 * @file f203_kem_enc_layout.h
 * @brief Alg.20 KEM Encaps I/O 尺寸（ml_kem_1024 / k=4）。
 */
#pragma once

#include "f203_encrypt_layout.h"

namespace F203KemEnc {

constexpr uint32_t kEkKemBytes = F203_EK_PKE_BYTES;
constexpr uint32_t kSharedSecretBytes = 32U;
constexpr uint32_t kCtBytes = F203_CT_PKE_BYTES;
constexpr uint32_t kSeedDBytes = 4U;
// 旁路 A（KEM_ENC_EXT_SEED，test-only）：host 直接提供 encaps_seed=m（32B），与 liboqs encaps_derand 对齐。
constexpr uint32_t kExtEncapsSeedBytes = 32U;
#if KEM_ENC_EXT_SEED
constexpr uint32_t kSeedGmBytes = kExtEncapsSeedBytes;
#else
constexpr uint32_t kSeedGmBytes = kSeedDBytes;
#endif
constexpr uint32_t kHashEkBytes = 32U;
constexpr uint32_t kGOutBytes = 64U;

}  // namespace F203KemEnc
