/**
 * @file f203_kem_kg_finish.hpp
 * @brief FIPS 203 Alg.19 / KeyGen_internal 尾段（设备侧）。
 *
 * 在 vendor PKE KeyGen 已产出 ek_pke/dk_pke 之后执行：
 *   1) UB 内得到 z（生产：DerandZFromSeedD；旁路 A：读 kem_seed[32:64]）；
 *   2) H(ek)=SHA3-256(ek_pke)；
 *   3) 按 liboqs 展开布局拼接 dk_kem，并复制 ek_kem。
 *
 * 本文件不含 PKE 矩阵/NTT；那些在 vendor/pke_keygen。
 */
#pragma once

#include "f203_kem_kg_derand_ub.hpp"
#include "f203_kem_kg_layout.h"

namespace F203KemKg {

/**
 * GM→GM 字节拷贝（AIV 标量循环；尾段数据量小，不做向量搬运）。
 * @param dst 目标 GM
 * @param src 源 GM
 * @param n   字节数
 */
__aicore__ inline void GmMemcpyU8(__gm__ uint8_t *dst, __gm__ const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

/**
 * Launch-3：KEM KeyGen 尾段实现（读 ek_pke/dk_pke，写 ek_kem/dk_kem）。
 *
 * @param seed_d_gm 生产：4B uint32 LE（与 prep 同源，仅用于 DerandZFromSeedD）；
 *                  旁路 A：64B kem_seed，本函数取后 32B 为 z
 * @param ek_pke_gm vendor compute 输出公钥 1568B
 * @param dk_pke_gm vendor compute 输出 PKE 私钥 1536B（即 sk/ŝ 编码）
 * @param ek_kem_gm 输出 KEM 公钥 1568B（内容等于 ek_pke）
 * @param dk_kem_gm 输出 3168B = dk_pke ‖ ek ‖ H(ek) ‖ z
 */
__aicore__ inline void KemKgFinishImpl(__gm__ uint8_t *seed_d_gm, __gm__ uint8_t *ek_pke_gm, __gm__ uint8_t *dk_pke_gm,
                                       __gm__ uint8_t *ek_kem_gm, __gm__ uint8_t *dk_kem_gm)
{
    // Alg.19：z 只在 UB 生成，禁止写独立 GM / debug 文件（SELF_CONTAINED）
    uint8_t z[kZBytes];
#if KEM_KG_EXT_SEED
    // 旁路 A（test-only）：seed_d_gm 承载 64B kem_seed = d(32)‖z(32)；finish 取后 32B 作 z。
    // 与 prep 取前 32B 作 d 对称，使两侧吃相同 host 随机字节；宏关时走 device 派生。
    const __gm__ uint8_t *seedBytes = reinterpret_cast<const __gm__ uint8_t *>(seed_d_gm);
    for (uint32_t i = 0; i < kZBytes; ++i) {
        z[i] = seedBytes[32U + i];
    }
#else
    // 生产：从 seed_d 域分离派生 z
    const uint32_t seed_d = *reinterpret_cast<__gm__ uint32_t *>(seed_d_gm);
    DerandZFromSeedD(seed_d, z);
#endif

    // ek_kem ← ek_PKE（字节级复制，无再编码）
    GmMemcpyU8(ek_kem_gm, ek_pke_gm, kEkKemBytes);

    // H(ek)：先把 ek 搬到 UB，再 SHA3-256（设备 SHA3 接口吃 UB 指针）
    uint8_t ek_ub[kEkKemBytes];
    for (uint32_t i = 0; i < kEkKemBytes; ++i) {
        ek_ub[i] = ek_pke_gm[i];
    }
    uint8_t h[kHashEkBytes];
    F203SeDeviceKeccak::Sha3OneShot(h, static_cast<int>(kHashEkBytes), ek_ub, kEkKemBytes);

    // dk_kem 拼接（liboqs 展开布局）：dk_pke ‖ ek ‖ H(ek) ‖ z
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
