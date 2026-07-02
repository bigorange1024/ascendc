/**
 * @file f203_kem_dec_fo.hpp
 * @brief Alg.18 K2：c 与 c' 比对 + 隐式拒绝 J(z‖c) + 写 K（设备 AIV 标量）。
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
 * FO 尾段：c_in 与 c' 相等则 K=K'，否则 K=J(z‖c)（SHAKE256，32B）。
 * @param c_in_gm 输入密文 c（1568B）
 * @param c_prime_gm 重加密 c'（pack 输出，同缓冲或另一 GM）
 * @param z_gm 32B 隐式拒绝秘密（dk_kem[3136:3168]）
 * @param Kprime_gm 32B G 输出前半
 * @param Kout_gm 32B 最终共享秘密
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
