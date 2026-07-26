/**
 * @file f203_kem_enc_layout.h
 * @brief Alg.20/17 Encaps（k=4）I/O 常量：与 stable Encrypt 公钥/密文几何对齐。
 *
 * 生产 I/O：input/ek_kem.bin(1568) + m.bin(32) + LUT → output/c.bin(1568) + K.bin(32)。
 * coins/r 仅 device workspace，不落盘。
 */
#pragma once

#include "f203_encrypt_prep_layout.h"
#include "f203_encrypt_tail_layout.h"

namespace F203KemEnc {

constexpr uint32_t kEkKemBytes = F203EncryptPrep::kEkPkeBytes;       // 1568
constexpr uint32_t kSharedSecretBytes = 32U;
constexpr uint32_t kCtBytes = F203_TAIL_C_BYTES;                     // 1568
constexpr uint32_t kMsgBytes = F203_TAIL_MSG_BYTES;                  // 32 = m
constexpr uint32_t kCoinsBytes = F203EncryptPrep::kCoinsBytes;       // 32 = r
constexpr uint32_t kHashEkBytes = 32U;
constexpr uint32_t kGOutBytes = 64U;                                 // SHA3-512 → K‖r

}  // namespace F203KemEnc
