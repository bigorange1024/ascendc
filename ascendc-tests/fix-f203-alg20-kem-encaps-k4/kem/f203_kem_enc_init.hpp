/**
 * @file f203_kem_enc_init.hpp
 * @brief Alg.17 头段：UB 内 m + H(ek) + G(m‖h) → K 与 Encrypt coins（r）。
 */
#pragma once

#include "f203_kem_enc_derand_ub.hpp"
#include "f203_kem_enc_layout.h"

namespace F203KemEnc {

__aicore__ inline void WriteGmBytes(__gm__ uint8_t *dst, const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

/**
 * Encaps_internal 设备头：写 K_gm、m_gm、coins_gm（G 后半）。
 * @param ek_gm   1568B 封装公钥
 * @param seed_gm 宏关：4B uint32 LE（SEED_D）→ DerandMFromSeedD；
 *                宏开（test-only）：32B encaps_seed=m，跳过 DerandMFromSeedD
 */
__aicore__ inline void KemEncInitHead(__gm__ uint8_t *ek_gm, __gm__ uint8_t *seed_gm, __gm__ uint8_t *K_gm,
                                      __gm__ uint8_t *m_gm, __gm__ uint8_t *coins_gm)
{
    uint8_t m[kHashEkBytes];
#if KEM_ENC_EXT_SEED
    // 旁路 A：seed_gm 承载 host encaps_seed（32B），device 仍算 H(ek)+G(m‖h)→K/coins
    const __gm__ uint8_t *seedBytes = reinterpret_cast<const __gm__ uint8_t *>(seed_gm);
    for (uint32_t i = 0U; i < kHashEkBytes; ++i) {
        m[i] = seedBytes[i];
    }
#else
    const uint32_t seed_d = *reinterpret_cast<__gm__ uint32_t *>(seed_gm);
    DerandMFromSeedD(seed_d, m);
#endif
    WriteGmBytes(m_gm, m, kHashEkBytes);

    uint8_t ek_ub[kEkKemBytes];
    for (uint32_t i = 0; i < kEkKemBytes; ++i) {
        ek_ub[i] = ek_gm[i];
    }
    uint8_t h[kHashEkBytes];
    F203SeDeviceKeccak::Sha3OneShot(h, static_cast<int>(kHashEkBytes), ek_ub, kEkKemBytes);

    uint8_t mh[64];
    for (uint32_t i = 0; i < kHashEkBytes; ++i) {
        mh[i] = m[i];
        mh[kHashEkBytes + i] = h[i];
    }
    uint8_t kr[kGOutBytes];
    F203SeDeviceKeccak::Sha3OneShot(kr, 64, mh, 64);

    WriteGmBytes(K_gm, kr, kSharedSecretBytes);
    WriteGmBytes(coins_gm, kr + kSharedSecretBytes, F203_ENC_COINS_BYTES);
}

}  // namespace F203KemEnc
