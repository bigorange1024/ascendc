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
constexpr uint32_t kHashEkBytes = 32U;
constexpr uint32_t kGOutBytes = 64U;

}  // namespace F203KemEnc
