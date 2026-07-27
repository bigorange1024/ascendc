/**
 * @file f203_kem_dec_phase_e_init.hpp
 * @brief Alg.18 行 6：G(m' ‖ h) → (K' ‖ r')；并入 Phase-E prep 前段（不另开 launch）。
 *
 * 对齐 Encaps `KemEncInitHead`，差异：
 *   - 不读 ek、不在此算 H(ek)；h 已由 dk 切片或 Phase-E-only 灌入；
 *   - 明文为 Decrypt 得到的 m'（非随机 m）。
 *
 * 输出：
 *   - K'（32B）供 FO 接受支路；
 *   - r'=coins（32B）供 EncryptPrep CBD。
 *
 * 背景：CT 要求 G 在设备侧；未采用 Host 算 G 再 H2D。
 */
#pragma once

#include "f203_kem_dec_layout.h"
#include "fips203_device_sha3.hpp"

namespace F203KemDec {

/** 将 Host/UB 侧字节写入 GM。 */
__aicore__ inline void WriteGmBytes(__gm__ uint8_t *dst, const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

/** 从 GM 读字节到局部缓冲。 */
__aicore__ inline void ReadGmBytes(uint8_t *dst, const __gm__ uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

/**
 * Phase-E 头：G(m'‖h)。
 * @param m_prime_gm 32B m'（Decrypt 输出）
 * @param h_gm       32B H(ek)（自 dk_kem[1568:1600)）
 * @param Kprime_gm  输出 K' 32B
 * @param coins_gm   输出 r' 32B → Encrypt prep
 * 前置：仅 block0（或 CPU 串行入口）调用，避免多核重复写 K'/coins。
 */
__aicore__ inline void KemDecPhaseEHead(__gm__ uint8_t *m_prime_gm, __gm__ uint8_t *h_gm,
                                        __gm__ uint8_t *Kprime_gm, __gm__ uint8_t *coins_gm)
{
    // 拼 64B 输入：m' ‖ h
    uint8_t mh[kGOutBytes];
    ReadGmBytes(mh, m_prime_gm, kHashBytes);
    ReadGmBytes(mh + kHashBytes, h_gm, kHashBytes);

    // SHA3-512：G → 64B = K'‖r'
    uint8_t kr[kGOutBytes];
    F203SeDeviceKeccak::Sha3OneShot(kr, static_cast<int>(kGOutBytes), mh, kGOutBytes);

    WriteGmBytes(Kprime_gm, kr, kSharedSecretBytes);
    WriteGmBytes(coins_gm, kr + kSharedSecretBytes, kCoinsBytes);
}

}  // namespace F203KemDec
