/**
 * @file f203_kem_dec_g.hpp
 * @brief Alg.18 K1：G(m'‖h) → K' 与 Encrypt coins r'（SHA3-512，设备 AIV 标量）。
 */
#pragma once

#include "f203_kem_dec_layout.h"
#include "fips203_device_sha3.hpp"

namespace F203KemDec {

__aicore__ inline void ReadGmBytes(uint8_t *dst, const __gm__ uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

__aicore__ inline void WriteGmBytes(__gm__ uint8_t *dst, const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

/**
 * Decaps_internal 段 K1：UB 内 m' + GM 读 h，写 K' 与 coins=r'。
 * @param m_ub 32B 明文（extract 留在 UB，禁止从 m_gm 标量回读）
 */
__aicore__ inline void KemDecGFromUb(const uint8_t *m_ub, __gm__ uint8_t *h_gm, __gm__ uint8_t *Kprime_gm,
                                     __gm__ uint8_t *coins_gm)
{
    uint8_t mh[kGOutBytes];
    for (uint32_t i = 0; i < kHashBytes; ++i) {
        mh[i] = m_ub[i];
    }
    ReadGmBytes(mh + kHashBytes, h_gm, kHashBytes);

    uint8_t kr[kGOutBytes];
    F203SeDeviceKeccak::Sha3OneShot(kr, static_cast<int>(kGOutBytes), mh, kGOutBytes);

    WriteGmBytes(Kprime_gm, kr, kSharedSecretBytes);
    WriteGmBytes(coins_gm, kr + kSharedSecretBytes, kCoinsBytes);
}

/**
 * 旧接口（仅 CPU 调试路径保留）；SIM 请用 KemDecGFromUb。
 */
__aicore__ inline void KemDecG(__gm__ uint8_t *m_gm, __gm__ uint8_t *h_gm, __gm__ uint8_t *Kprime_gm,
                             __gm__ uint8_t *coins_gm)
{
    uint8_t m_ub[kHashBytes];
    ReadGmBytes(m_ub, m_gm, kHashBytes);
    KemDecGFromUb(m_ub, h_gm, Kprime_gm, coins_gm);
}

}  // namespace F203KemDec
