/**
 * @file f203_kem_enc_derand_ub.hpp
 * @brief Alg.20 随机性 m：device UB 内 SHA3-256 域分离派生。
 *
 * 背景：用户锁定 m 在 device 生成；Host 仅 seed_d；消息格式与 golden/liboqs fixture 对齐。
 */
#pragma once

#include "fips203_device_sha3.hpp"

namespace F203KemEnc {

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

/** SEED_D → SHA3-256 → m[32]；前缀 exp-mlkem-f203-kem-encaps-k4:SEED_M= */
__aicore__ inline void DerandMFromSeedD(uint32_t seed_d, uint8_t m[32])
{
    char msg[56];
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
    msg[pos++] = 'e';
    msg[pos++] = 'n';
    msg[pos++] = 'c';
    msg[pos++] = 'a';
    msg[pos++] = 'p';
    msg[pos++] = 's';
    msg[pos++] = '-';
    msg[pos++] = 'k';
    msg[pos++] = '4';
    msg[pos++] = ':';
    msg[pos++] = 'S';
    msg[pos++] = 'E';
    msg[pos++] = 'E';
    msg[pos++] = 'D';
    msg[pos++] = '_';
    msg[pos++] = 'M';
    msg[pos++] = '=';
    pos += U32ToDec(seed_d, msg + pos);
    F203SeDeviceKeccak::Sha3OneShot(m, 32, reinterpret_cast<const uint8_t *>(msg), static_cast<uint32_t>(pos));
}

}  // namespace F203KemEnc
