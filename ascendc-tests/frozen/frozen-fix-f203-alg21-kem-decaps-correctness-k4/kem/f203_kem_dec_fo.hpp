/**
 * @file f203_kem_dec_fo.hpp
 * @brief FIPS 203 Alg.21 / Decaps_internal 设备 FO 尾段（规范中的密文比对 + 隐式拒绝）。
 *
 * 对应：若 c'==c 则 K=K'，否则 K=J(z‖c)=SHAKE256(z‖c,32)。
 * 在 f203_kem_dec_pack 写完 c' 后同核调用，保证 CPU/SIM 生产路径均为设备 FO
 *（无 host memcmp）。拒绝路径测试见 KEM_DECAPS_TAMPER_C（篡改 coins 使 c'≠c）。
 */
#pragma once

#include "f203_kem_dec_layout.h"
#include "fips203_device_sha3.hpp"

namespace F203KemDec {

/**
 * 常量时间风格的 GM 字节相等：累积 XOR，最后判零。
 * @param a / b GM 指针
 * @param n 字节数（通常 kCtBytes）
 */
__aicore__ inline bool CtEqualGm(const __gm__ uint8_t *a, const __gm__ uint8_t *b, uint32_t n)
{
    uint8_t diff = 0U;
    for (uint32_t i = 0; i < n; ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0U;
}

/**
 * FO 尾段：c_in 与 c' 相等则 K=K'，否则 K=J(z‖c)。
 *
 * @param c_in_gm    输入密文 c（1568B）
 * @param c_prime_gm 重加密 c'（pack 输出）
 * @param z_gm       32B 隐式拒绝秘密（dk_kem[3136:3168]）
 * @param Kprime_gm  32B G 输出前半 K'
 * @param Kout_gm    32B 最终共享秘密
 */
__aicore__ inline void KemDecFo(__gm__ uint8_t *c_in_gm, __gm__ uint8_t *c_prime_gm, __gm__ uint8_t *z_gm,
                                __gm__ uint8_t *Kprime_gm, __gm__ uint8_t *Kout_gm)
{
    const bool accept = CtEqualGm(c_in_gm, c_prime_gm, kCtBytes);

    // 始终计算拒绝分支 J(z‖c)，再用 mask 选择，避免分支泄露（设备标量实现）
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
