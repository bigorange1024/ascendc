/**
 * @file f203_kem_dec_fo.hpp
 * @brief Alg.18 行 8–12 设备 FO：c≟c' → K' 或 J(z‖c)=SHAKE256(z‖c,32)。
 *
 * 落点：pack 写完 c' 后同核调用（CPU：pack_fo 核；SIM T19i：l18_l19 尾，见 INTEGRATION_PLAN §7）。
 */
#pragma once

#include "f203_kem_dec_layout.h"
#include "fips203_device_sha3.hpp"

namespace F203KemDec {

__aicore__ inline bool CtEqualGm(const __gm__ uint8_t *a, const __gm__ uint8_t *b, uint32_t n)
{
    uint8_t diff = 0U;
    for (uint32_t i = 0; i < n; ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0U;
}

/**
 * @param c_in_gm    输入密文 c
 * @param c_prime_gm 重加密 c'
 * @param z_gm       32B z
 * @param Kprime_gm  32B K'
 * @param Kout_gm    32B 最终 K
 */
__aicore__ inline void KemDecFo(__gm__ uint8_t *c_in_gm, __gm__ uint8_t *c_prime_gm, __gm__ uint8_t *z_gm,
                                __gm__ uint8_t *Kprime_gm, __gm__ uint8_t *Kout_gm)
{
    const bool accept = CtEqualGm(c_in_gm, c_prime_gm, kCtBytes);

    constexpr uint32_t kZcLen = kHashBytes + kCtBytes;
    uint8_t zc[kZcLen];
    for (uint32_t i = 0; i < kHashBytes; ++i) {
        zc[i] = z_gm[i];
    }
    for (uint32_t i = 0; i < kCtBytes; ++i) {
        zc[kHashBytes + i] = c_in_gm[i];
    }

    uint8_t k_reject[kSharedSecretBytes];
    F203SeDeviceKeccak::Shake256OneShot(k_reject, kSharedSecretBytes, zc, kHashBytes + kCtBytes);

    uint8_t k_prime[kSharedSecretBytes];
    for (uint32_t i = 0; i < kSharedSecretBytes; ++i) {
        k_prime[i] = Kprime_gm[i];
    }

    const uint8_t mask = accept ? 0xFFU : 0U;
    for (uint32_t i = 0; i < kSharedSecretBytes; ++i) {
        Kout_gm[i] = static_cast<uint8_t>((k_prime[i] & mask) | (k_reject[i] & static_cast<uint8_t>(~mask)));
    }
}

}  // namespace F203KemDec
