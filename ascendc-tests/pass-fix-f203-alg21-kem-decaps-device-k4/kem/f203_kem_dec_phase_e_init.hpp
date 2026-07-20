/**
 * @file f203_kem_dec_phase_e_init.hpp
 * @brief Alg.18 行 6：G(m' ‖ h) → K' ‖ r'；并入 Phase-E prep 前段（不另开 launch）。
 *
 * 对齐 Encaps 的 KemEncInitHead，但：
 *   - 不读 ek、不算 H(ek)；h 由 Host/Phase-D 提供；
 *   - 输入明文为 m'（Decrypt 输出或 Phase-E-only 灌入）。
 */
#pragma once

#include "f203_kem_dec_layout.h"
#include "fips203_device_sha3.hpp"

namespace F203KemDec {

__aicore__ inline void WriteGmBytes(__gm__ uint8_t *dst, const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

__aicore__ inline void ReadGmBytes(uint8_t *dst, const __gm__ uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

/**
 * Phase-E 头：仅 block0 / CPU 串行入口。
 * @param m_prime_gm 32B m'
 * @param h_gm       32B H(ek)（自 dk）
 * @param Kprime_gm  输出 K' 32B（FO 用；非最终 K）
 * @param coins_gm   输出 r' 32B → Encrypt prep CBD
 */
__aicore__ inline void KemDecPhaseEHead(__gm__ uint8_t *m_prime_gm, __gm__ uint8_t *h_gm,
                                        __gm__ uint8_t *Kprime_gm, __gm__ uint8_t *coins_gm)
{
    uint8_t mh[kGOutBytes];
    ReadGmBytes(mh, m_prime_gm, kHashBytes);
    ReadGmBytes(mh + kHashBytes, h_gm, kHashBytes);

    uint8_t kr[kGOutBytes];
    F203SeDeviceKeccak::Sha3OneShot(kr, static_cast<int>(kGOutBytes), mh, kGOutBytes);

    WriteGmBytes(Kprime_gm, kr, kSharedSecretBytes);
    WriteGmBytes(coins_gm, kr + kSharedSecretBytes, kCoinsBytes);
}

}  // namespace F203KemDec
