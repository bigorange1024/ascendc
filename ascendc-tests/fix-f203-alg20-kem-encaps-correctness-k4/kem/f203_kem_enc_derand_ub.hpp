/**
 * @file f203_kem_enc_derand_ub.hpp
 * @brief FIPS 203 Alg.20 随机性 m：device UB 内 SHA3-256 域分离派生。
 *
 * 与 vendor Encrypt 关系：本函数只产出 m[32]；后续 H(ek)、G(m‖h)→K‖r 在
 * f203_kem_enc_init.hpp；r 再交给 vendor prep_re（coins→r/e₁/e₂）。
 *
 * 背景（已锁定）：m 在 device 生成，Host 仅 seed_d；禁止生产路径落盘 m。
 * 消息前缀与 scripts/gen_data.py / liboqs fixture 对齐。
 */
#pragma once

#include "fips203_device_sha3.hpp"

namespace F203KemEnc {

/**
 * uint32 → 十进制 ASCII（无堆；与 KeyGen DerandZ / vendor DerandFromSeedD 同型）。
 * @param v   SEED_D
 * @param out 输出缓冲（≥10B）
 * @return 写入字符数
 */
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
 * SEED_D → SHA3-256 → m[32]。
 * 前缀固定 "exp-mlkem-f203-kem-encaps-k4:SEED_M="（与 host golden 一致）。
 * @param seed_d Host 可复现种子
 * @param m      输出 32B 明文随机性（留 UB / 写 m_gm）
 */
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