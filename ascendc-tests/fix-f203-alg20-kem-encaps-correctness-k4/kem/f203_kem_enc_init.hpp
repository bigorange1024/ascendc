/**
 * @file f203_kem_enc_init.hpp
 * @brief FIPS 203 Alg.20 / Encaps_internal 设备头段（对应规范中的 m、H、G）。
 *
 * 步骤（均在 AIV UB）：
 *   1) 得到 m（生产 DerandMFromSeedD；旁路 A 读 encaps_seed）
 *   2) h = H(ek) = SHA3-256(ek)
 *   3) (K ‖ r) = G(m‖h) = SHA3-512(m‖h)
 * 随后 vendor Encrypt 用 r 作 coins；K 直接作为 Encaps 共享秘密输出。
 *
 * 未采用：Host 预填 coins.bin（SELF_CONTAINED：r 须来自 device G）。
 */
#pragma once

#include "f203_kem_enc_derand_ub.hpp"
#include "f203_kem_enc_layout.h"

namespace F203KemEnc {

/** UB→GM 字节写（标量）。 */
__aicore__ inline void WriteGmBytes(__gm__ uint8_t *dst, const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

/**
 * Encaps_internal 设备头：写 K_gm、m_gm、coins_gm（G 后半 = Encrypt 的 r）。
 *
 * @param ek_gm   1568B 封装公钥（Alg.19 ek_kem）
 * @param seed_gm 宏关：4B uint32 LE（SEED_D）→ DerandMFromSeedD；
 *                宏开（test-only）：32B encaps_seed=m，跳过 DerandMFromSeedD
 * @param K_gm    输出共享秘密 32B
 * @param m_gm    输出 m（供后续 g4_noise 编码进 v）
 * @param coins_gm 输出 r=coins 32B（喂 vendor BuildReFromCoinsGm）
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
    // 生产：域分离派生 m
    const uint32_t seed_d = *reinterpret_cast<__gm__ uint32_t *>(seed_gm);
    DerandMFromSeedD(seed_d, m);
#endif
    WriteGmBytes(m_gm, m, kHashEkBytes);

    // h = H(ek)：ek 先入 UB 再 SHA3-256
    uint8_t ek_ub[kEkKemBytes];
    for (uint32_t i = 0; i < kEkKemBytes; ++i) {
        ek_ub[i] = ek_gm[i];
    }
    uint8_t h[kHashEkBytes];
    F203SeDeviceKeccak::Sha3OneShot(h, static_cast<int>(kHashEkBytes), ek_ub, kEkKemBytes);

    // mh = m‖h，G = SHA3-512 → kr[64] = K‖r
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