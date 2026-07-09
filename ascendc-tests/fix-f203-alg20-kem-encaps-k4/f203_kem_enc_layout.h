/**
 * @file f203_kem_enc_layout.h
 * @brief FIPS 203 Alg.20（ML-KEM.Encaps）I/O 尺寸：ml_kem_1024 / k=4。
 *
 * 流水线：input/ek_kem.bin（来自 Alg.19）+ seed → device 内 m/H/G + vendor Encrypt G5
 * → output/c.bin(1568) + K.bin(32)。本头只定 KEM 侧常量；PKE 密文/公钥尺寸复用
 * vendor f203_encrypt_layout.h（F203_EK_PKE_BYTES / F203_CT_PKE_BYTES）。
 */
#pragma once

#include "f203_encrypt_layout.h"

namespace F203KemEnc {

/** 封装公钥长度（与 ek_PKE 相同，1568B）。 */
constexpr uint32_t kEkKemBytes = F203_EK_PKE_BYTES;
/** 共享秘密 K 长度。 */
constexpr uint32_t kSharedSecretBytes = 32U;
/** 密文 c 长度（与 PKE Encrypt 输出相同）。 */
constexpr uint32_t kCtBytes = F203_CT_PKE_BYTES;
/** 生产路径 seed_d.bin：4B uint32 LE。 */
constexpr uint32_t kSeedDBytes = 4U;
// 旁路 A（KEM_ENC_EXT_SEED，test-only）：host 直接提供 encaps_seed=m（32B），与 liboqs encaps_derand 对齐。
constexpr uint32_t kExtEncapsSeedBytes = 32U;
#if KEM_ENC_EXT_SEED
constexpr uint32_t kSeedGmBytes = kExtEncapsSeedBytes;
#else
constexpr uint32_t kSeedGmBytes = kSeedDBytes;
#endif
/** H(ek) / m 长度。 */
constexpr uint32_t kHashEkBytes = 32U;
/** G(m‖H(ek)) = SHA3-512 输出 64B → K‖r。 */
constexpr uint32_t kGOutBytes = 64U;

}  // namespace F203KemEnc