/**
 * @file f203_kem_dec_fo.hpp
 * @brief FIPS 203 Alg.18 行 8–12 设备侧 FO（隐式拒绝）：比较 c≟c'，输出最终共享秘密 K。
 *
 * 流水线位置（Alg.21 Decaps）：
 *   Phase-D Decrypt → m' → Phase-E：G(m'‖h)→K'‖r' → Encrypt(ek,m';r')→c'
 *   → 本模块 FO → K ∈ {K', J(z‖c)}。
 *
 * 落点：
 *   - CPU：`f203_kem_dec_pack_fo` 写完 c' 后同核调用；
 *   - SIM：探针本地 `f203_encrypt_l18_l19` 尾（T19i），四 FO 指针皆非空才启用。
 *
 * 与 golden：仅验 output/K.bin I/O 等价；禁止与 correctness vendor 源码同构验收。
 * 背景：设备侧 FO（非 host memcmp）；交付默认 SIM 1-session（T2）。
 */
#pragma once

#include "f203_kem_dec_layout.h"
#include "fips203_device_sha3.hpp"

namespace F203KemDec {

/**
 * 恒定时间式字节串相等：逐字节 XOR 累或，避免早退分支泄露（实现为标量环，非密码学 CT 证明）。
 * @param a,b  GM 上长度为 n 的字节区
 * @param n    比较长度（密文为 kCtBytes=768）
 * @return true 当且仅当全程相等
 */
__aicore__ inline bool CtEqualGm(const __gm__ uint8_t *a, const __gm__ uint8_t *b, uint32_t n)
{
    uint8_t diff = 0U;
    // 累或：任一位不同则 diff≠0；不按字节早退
    for (uint32_t i = 0; i < n; ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0U;
}

/**
 * Alg.18 FO：accept=(c==c') ? 选 K' : 选 J(z‖c)=SHAKE256(z‖c, 32)。
 *
 * @param c_in_gm    输入密文 c（768B，GM）
 * @param c_prime_gm 重加密密文 c'（pack 刚写出，GM）
 * @param z_gm       拒绝种子 z（32B，来自 dk_kem 尾）
 * @param Kprime_gm  候选共享秘密 K'（32B，来自 G(m'‖h) 高半）
 * @param Kout_gm    最终 K（32B，写回 GM → Host D2H）
 *
 * 前置：调用方已保证上述指针有效；通常仅 block0 / 单 AIV 调用。
 */
__aicore__ inline void KemDecFo(__gm__ uint8_t *c_in_gm, __gm__ uint8_t *c_prime_gm, __gm__ uint8_t *z_gm,
                                __gm__ uint8_t *Kprime_gm, __gm__ uint8_t *Kout_gm)
{
    // 行 8–9：密文相等判定（隐式拒绝的唯一门闩）
    const bool accept = CtEqualGm(c_in_gm, c_prime_gm, kCtBytes);

    // 拼 z‖c（1600B）供 SHAKE256；缓冲区在 UB/栈侧，长度编译期常量
    constexpr uint32_t kZcLen = kHashBytes + kCtBytes;
    uint8_t zc[kZcLen];
    for (uint32_t i = 0; i < kHashBytes; ++i) {
        zc[i] = z_gm[i];
    }
    for (uint32_t i = 0; i < kCtBytes; ++i) {
        zc[kHashBytes + i] = c_in_gm[i];
    }

    // 拒绝支路：J(z‖c) = SHAKE256(z‖c, 32)
    uint8_t k_reject[kSharedSecretBytes];
    F203SeDeviceKeccak::Shake256OneShot(k_reject, kSharedSecretBytes, zc, kHashBytes + kCtBytes);

    // 接受支路候选：K'（已由 Phase-E Head 写入 GM）
    uint8_t k_prime[kSharedSecretBytes];
    for (uint32_t i = 0; i < kSharedSecretBytes; ++i) {
        k_prime[i] = Kprime_gm[i];
    }

    // 无分支选通：accept→全 1 mask 取 K'，否则取 J
    const uint8_t mask = accept ? 0xFFU : 0U;
    for (uint32_t i = 0; i < kSharedSecretBytes; ++i) {
        Kout_gm[i] = static_cast<uint8_t>((k_prime[i] & mask) | (k_reject[i] & static_cast<uint8_t>(~mask)));
    }
}

}  // namespace F203KemDec
