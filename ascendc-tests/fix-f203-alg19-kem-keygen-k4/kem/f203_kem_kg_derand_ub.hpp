/**
 * @file f203_kem_kg_derand_ub.hpp
 * @brief Alg.19 随机性 z：device UB 内 SHA3-256 域分离派生（与 d 的 DerandFromSeedD 对称）。
 *
 * 背景：用户锁定 d/z 均在 device AscendC 生成，禁止 D2H/落盘本体；可复现验收 Host 仅给 seed_d。
 * d 由 vendor prep 内 DerandFromSeedD 消费；本文件仅负责 z。
 */
#pragma once

#include "fips203_device_sha3.hpp"

namespace F203KemKg {

/** 无除法 uint32 → 十进制 ASCII（与 vendor alg7 DerandFromSeedD 同型）。 */
__aicore__ inline int U32ToDec(uint32_t v, char *out)
{
    char tmp[10];
    int n = 0;
    if (v == 0U) {
        tmp[n++] = '0';
    } else {
        while (v > 0U) {
            tmp[n++] = static_cast<char>('0' + (v % 10U));
            v /= 10U;
        }
    }
    for (int i = 0; i < n; ++i) {
        out[i] = tmp[n - 1 - i];
    }
    return n;
}

/**
 * SEED_D → 域分离消息 → SHA3-256 → z[32]。
 * 消息前缀固定为 exp-mlkem-f203-kem-k4:SEED_Z=（与 host golden / liboqs fixture 对齐）。
 */
__aicore__ inline void DerandZFromSeedD(uint32_t seed_d, uint8_t z[32])
{
    char msg[48];
    int pos = 0;
    msg[pos++] = 'e';
    msg[pos++] = 'x';
    msg[pos++] = 'p';
    msg[pos++] = '-';
    msg[pos++] = 'm';
    msg[pos++] = 'l';
    msg[pos++] = 'k';
    msg[pos++] = 'e';
    msg[pos++] = 'm';
    msg[pos++] = '-';
    msg[pos++] = 'f';
    msg[pos++] = '2';
    msg[pos++] = '0';
    msg[pos++] = '3';
    msg[pos++] = '-';
    msg[pos++] = 'k';
    msg[pos++] = 'e';
    msg[pos++] = 'm';
    msg[pos++] = '-';
    msg[pos++] = 'k';
    msg[pos++] = '4';
    msg[pos++] = ':';
    msg[pos++] = 'S';
    msg[pos++] = 'E';
    msg[pos++] = 'E';
    msg[pos++] = 'D';
    msg[pos++] = '_';
    msg[pos++] = 'Z';
    msg[pos++] = '=';
    pos += U32ToDec(seed_d, msg + pos);
    F203SeDeviceKeccak::Sha3OneShot(z, 32, reinterpret_cast<const uint8_t *>(msg), static_cast<uint32_t>(pos));
}

}  // namespace F203KemKg
