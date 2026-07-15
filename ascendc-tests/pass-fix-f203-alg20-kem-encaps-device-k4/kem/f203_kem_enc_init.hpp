/**
 * @file f203_kem_enc_init.hpp
 * @brief FIPS 203 Alg.17 设备头：读外部 m，算 H(ek)/G(m‖h)，写 K 与 coins(=r)。
 *
 * 并入 prep 入口前段（不另开 launch）。背景：用户锁定 m 为 GM 输入；r 禁止 Host 预填。
 * SHA3：shared `F203SeDeviceKeccak::Sha3OneShot`（256/512）。
 */
#pragma once

#include "f203_kem_enc_layout.h"
#include "fips203_device_sha3.hpp"

namespace F203KemEnc {

/** UB→GM 逐字节写（标量；头段落点数据 ≤64B）。 */
__aicore__ inline void WriteGmBytes(__gm__ uint8_t *dst, const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

/**
 * Encaps_internal 头段（仅 AIV0 / block0 调用）。
 *
 * @param ek_gm    1568B 封装公钥（= ek_PKE）
 * @param m_gm     输入 32B 消息（外部读入；生产勿导出）
 * @param K_gm     输出共享秘密 32B
 * @param coins_gm 输出 r=G 后半 32B（喂 Encrypt prep CBD）
 */
__aicore__ inline void KemEncInitHead(__gm__ uint8_t *ek_gm, __gm__ uint8_t *m_gm, __gm__ uint8_t *K_gm,
                                      __gm__ uint8_t *coins_gm)
{
    // —— ① 读外部 m[32] ——
    uint8_t m[kHashEkBytes];
    for (uint32_t i = 0U; i < kHashEkBytes; ++i) {
        m[i] = m_gm[i];
    }

    // —— ② h = H(ek) = SHA3-256(ek[1568]) ——
    uint8_t ek_ub[kEkKemBytes];
    for (uint32_t i = 0U; i < kEkKemBytes; ++i) {
        ek_ub[i] = ek_gm[i];
    }
    uint8_t h[kHashEkBytes];
    F203SeDeviceKeccak::Sha3OneShot(h, static_cast<int>(kHashEkBytes), ek_ub, kEkKemBytes);

    // —— ③ (K‖r) = G(m‖h) = SHA3-512 ——
    uint8_t mh[64];
    for (uint32_t i = 0U; i < kHashEkBytes; ++i) {
        mh[i] = m[i];
        mh[kHashEkBytes + i] = h[i];
    }
    uint8_t kr[kGOutBytes];
    F203SeDeviceKeccak::Sha3OneShot(kr, 64, mh, 64);

    WriteGmBytes(K_gm, kr, kSharedSecretBytes);
    WriteGmBytes(coins_gm, kr + kSharedSecretBytes, kCoinsBytes);
}

}  // namespace F203KemEnc
