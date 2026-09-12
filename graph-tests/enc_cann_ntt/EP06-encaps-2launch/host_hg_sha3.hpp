/**
 * @file host_hg_sha3.hpp
 * @brief Host 侧 H=SHA3-256、G=SHA3-512（tiny_sha3；Encaps Alg.16/20 壳）
 *
 * 背景：EP01 仅 Host 哈希 + 桩 Encrypt；设备 SHA3 后置 EP03。
 * 未采用 OpenSSL；禁抄 examples/frozen/ER 设备核。
 */
#pragma once

#include <cstdint>
#include <cstring>

extern "C" {
#include "sha3.h"
}

namespace host_hg {

/** H(ek)：SHA3-256 → 32B。 */
inline void H_Sha3_256(uint8_t *out32, const uint8_t *ek, size_t ekLen)
{
    sha3(ek, ekLen, out32, 32);
}

/**
 * G(m‖h)：SHA3-512 → (K̄[32], r[32])。
 * @param m  32B；@param h  32B = H(ek)
 */
inline void G_Sha3_512(uint8_t *kBar32, uint8_t *r32, const uint8_t *m, const uint8_t *h)
{
    uint8_t msg[64];
    std::memcpy(msg, m, 32);
    std::memcpy(msg + 32, h, 32);
    uint8_t dig[64];
    sha3(msg, 64, dig, 64);
    std::memcpy(kBar32, dig, 32);
    std::memcpy(r32, dig + 32, 32);
}

} // namespace host_hg
