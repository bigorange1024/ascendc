/**
 * @file f203_kem_kg_finish.hpp
 * @brief Alg.19 / KeyGen_internal 尾段：H(ek)、UB 内采 z、拼接 liboqs 布局 dk_kem（3168B）。
 */
#pragma once

#include "f203_kem_kg_derand_ub.hpp"
#include "f203_kem_kg_layout.h"

namespace F203KemKg {

/** GM 字节拷贝（AIV 标量）。 */
__aicore__ inline void GmMemcpyU8(__gm__ uint8_t *dst, __gm__ const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

/**
 * Launch-3：KEM 尾段（读 ek_pke/dk_pke，写 ek_kem/dk_kem）。
 * @param seed_d_gm 4B uint32 LE（与 prep 同源；仅用于 UB 内 DerandZFromSeedD）
 * @param ek_pke_gm 1568B
 * @param dk_pke_gm 1536B
 * @param ek_kem_gm 1568B 输出
 * @param dk_kem_gm 3168B 输出 = dk_pke ‖ ek ‖ H(ek) ‖ z
 */
__aicore__ inline void KemKgFinishImpl(__gm__ uint8_t *seed_d_gm, __gm__ uint8_t *ek_pke_gm, __gm__ uint8_t *dk_pke_gm,
                                       __gm__ uint8_t *ek_kem_gm, __gm__ uint8_t *dk_kem_gm)
{
    const uint32_t seed_d = *reinterpret_cast<__gm__ uint32_t *>(seed_d_gm);

    // Alg.19：z 在 UB 生成，禁止写独立 GM / debug 文件
    uint8_t z[kZBytes];
    DerandZFromSeedD(seed_d, z);

    // ek_kem ← ek_PKE
    GmMemcpyU8(ek_kem_gm, ek_pke_gm, kEkKemBytes);

    // H(ek)：ek 先搬到 UB 再 SHA3-256（Hash = SHA3-256，32B）
    uint8_t ek_ub[kEkKemBytes];
    for (uint32_t i = 0; i < kEkKemBytes; ++i) {
        ek_ub[i] = ek_pke_gm[i];
    }
    uint8_t h[kHashEkBytes];
    F203SeDeviceKeccak::Sha3OneShot(h, static_cast<int>(kHashEkBytes), ek_ub, kEkKemBytes);

    // dk_kem 拼接（liboqs 展开布局）
    GmMemcpyU8(dk_kem_gm, dk_pke_gm, kDkPkeBytes);
    GmMemcpyU8(dk_kem_gm + kDkPkeBytes, ek_pke_gm, kEkKemBytes);
    for (uint32_t i = 0; i < kHashEkBytes; ++i) {
        dk_kem_gm[kDkPkeBytes + kEkKemBytes + i] = h[i];
    }
    for (uint32_t i = 0; i < kZBytes; ++i) {
        dk_kem_gm[kDkPkeBytes + kEkKemBytes + kHashEkBytes + i] = z[i];
    }
}

}  // namespace F203KemKg
