/**
 * @file f203_kem_dec_layout.h
 * @brief Alg.21/18 Decaps（k=4）I/O 常量；与 stable Encrypt/Decrypt 几何对齐。
 *
 * Phase-E-only：input/{m_prime,h,z,ek_kem,c} + LUT → output/K.bin（可选 dump c'）。
 */
#pragma once

#include "f203_encrypt_prep_layout.h"
#include "f203_encrypt_tail_layout.h"

namespace F203KemDec {

constexpr uint32_t kEkKemBytes = F203EncryptPrep::kEkPkeBytes;  // 1568
constexpr uint32_t kDkKemBytes = 3168U;
constexpr uint32_t kDkPkeBytes = 1536U;
constexpr uint32_t kSharedSecretBytes = 32U;
constexpr uint32_t kCtBytes = F203_TAIL_C_BYTES;    // 1568
constexpr uint32_t kMsgBytes = F203_TAIL_MSG_BYTES; // 32 = m'
constexpr uint32_t kCoinsBytes = F203EncryptPrep::kCoinsBytes;
constexpr uint32_t kHashBytes = 32U;
constexpr uint32_t kGOutBytes = 64U;

/** dk_kem 切片偏移（与 alg19 / liboqs 一致）。 */
constexpr uint32_t kOffEk = kDkPkeBytes;                 // 1536
constexpr uint32_t kOffH = kOffEk + kEkKemBytes;         // 3104
constexpr uint32_t kOffZ = kOffH + kHashBytes;           // 3136

}  // namespace F203KemDec
