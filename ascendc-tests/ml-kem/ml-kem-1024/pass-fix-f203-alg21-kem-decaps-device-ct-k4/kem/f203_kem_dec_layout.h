/**
 * @file f203_kem_dec_layout.h
 * @brief Alg.21 / Alg.18 Decaps（ml_kem_1024，k=4）I/O 与 dk_kem 切片常量。
 *
 * 几何对齐：
 *   - 密文/公钥长度与 stable Encrypt `F203_TAIL_*` / EncryptPrep 一致；
 *   - dk_kem=3168B 布局与 Alg.19 KeyGen / liboqs 一致（见偏移表）。
 *
 * 典型 I/O：
 *   全链：input/{dk_kem,c,Decrypt LUT,Encrypt LUT} → output/{m_prime,K,c_prime?}
 *   Phase-E-only：input/{m_prime,h,z,ek_kem|ek_pke,c} + LUT → output/K.bin
 *
 * 禁止擅自改字节数绕过对拍（锁参）。
 */
#pragma once

#include "f203_encrypt_prep_layout.h"
#include "f203_encrypt_tail_layout.h"

namespace F203KemDec {

constexpr uint32_t kEkKemBytes = F203EncryptPrep::kEkPkeBytes;  // 1568：ek（=ek_pke 本体）
constexpr uint32_t kDkKemBytes = 3168U;                         // dk_kem 全长
constexpr uint32_t kDkPkeBytes = 1536U;                         // dk_pke = dk_kem 前缀
constexpr uint32_t kSharedSecretBytes = 32U;                    // K / K'
constexpr uint32_t kCtBytes = F203_TAIL_C_BYTES;                // 1568：c / c'
constexpr uint32_t kMsgBytes = F203_TAIL_MSG_BYTES;             // 32：m'
constexpr uint32_t kCoinsBytes = F203EncryptPrep::kCoinsBytes;  // 32：r'（G 低半）
constexpr uint32_t kHashBytes = 32U;                            // h、z、SHA3 块
constexpr uint32_t kGOutBytes = 64U;                            // G 输出：K'‖r'

/**
 * dk_kem 字节布局（与 alg19 / liboqs ML-KEM-1024 一致）：
 *   [0, 1536)     dk_pke
 *   [1536, 3104)  ek（1568）
 *   [3104, 3136)  h = H(ek)（32）
 *   [3136, 3168)  z（32）
 *
 * 行 1–4 切片：Host 用指针偏移，不另开 launch。
 */
constexpr uint32_t kOffEk = kDkPkeBytes;                 // 1536
constexpr uint32_t kOffH = kOffEk + kEkKemBytes;         // 3104
constexpr uint32_t kOffZ = kOffH + kHashBytes;           // 3136

}  // namespace F203KemDec
