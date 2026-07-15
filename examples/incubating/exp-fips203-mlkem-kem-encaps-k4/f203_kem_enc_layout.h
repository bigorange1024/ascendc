/**
 * @file f203_kem_enc_layout.h
 * @brief Alg.20/17 Encaps（k=4）I/O 常量：与 vendored Encrypt 公钥/密文几何对齐。
 *
 * 生产 I/O：input/ek_kem.bin(1568) + m.bin(32) + LUT → output/c.bin(1568) + K.bin(32)。
 * 标准变量：$m$（输入）、$r$（设备 G 后半，非 Host 输入）、$K$、$c$。
 * customspec：exp-fips203-mlkem-kem-encaps-k4-实现方案-customspec.*
 */
#pragma once

#include "f203_encrypt_prep_layout.h"
#include "f203_encrypt_tail_layout.h"

namespace F203KemEnc {

constexpr uint32_t kEkKemBytes = F203EncryptPrep::kEkPkeBytes;  // 1568 = ek_PKE
constexpr uint32_t kSharedSecretBytes = 32U;                   // K
constexpr uint32_t kCtBytes = F203_TAIL_C_BYTES;                // 1568 = c
constexpr uint32_t kMsgBytes = F203_TAIL_MSG_BYTES;             // 32 = m
constexpr uint32_t kRBytes = F203EncryptPrep::kCoinsBytes;      // 32 = r（Alg.14 随机性）
constexpr uint32_t kHashEkBytes = 32U;                          // h = H(ek)
constexpr uint32_t kGOutBytes = 64U;                            // SHA3-512 → K‖r

}  // namespace F203KemEnc
