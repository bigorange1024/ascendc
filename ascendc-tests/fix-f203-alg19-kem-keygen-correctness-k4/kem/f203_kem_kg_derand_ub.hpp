/**
 * @file f203_kem_kg_derand_ub.hpp
 * @brief FIPS 203 Alg.19 随机性 z：在 device UB 内用 SHA3-256 做域分离派生。
 *
 * 与 PKE vendor 段关系：
 *   - d（KeyGen 主随机性）由 vendor/pke_keygen prep 内 DerandFromSeedD 消费；
 *   - 本文件只派生 z，供 Launch-3 KemKgFinish 写入 dk_kem 尾 32B。
 *
 * 背景（已锁定）：d/z 均在 AscendC 生成，禁止 D2H/落盘随机性本体；
 * Host 可复现验收只给 seed_d.bin（4B）。旁路 A（KEM_KG_EXT_SEED）不走本函数。
 */
#pragma once

#include "fips203_device_sha3.hpp"

namespace F203KemKg {

/**
 * 将 uint32 写成十进制 ASCII（无堆分配；与 vendor alg7 DerandFromSeedD 同型）。
 * @param v   待转换无符号整数（SEED_D）
 * @param out 输出缓冲，调用方保证 ≥10 字节
 * @return 写入的字符数（不含 '\\0'）
 */
__aicore__ inline int U32ToDec(uint32_t v, char *out)
{
    char tmp[10];
    int n = 0;
    // 特殊：0 直接写 '0'，避免 while 不进入
    if (v == 0U) {
        tmp[n++] = '0';
    } else {
        // 低位先入 tmp，再反转到 out
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
 * 由 SEED_D 派生 Alg.19 的 z[32]。
 *
 * 消息 = 固定前缀 "exp-mlkem-f203-kem-k4:SEED_Z=" ‖ 十进制(seed_d)，
 * 再 SHA3-256 → 32B。前缀与 host golden / liboqs fixture 对齐（见 scripts/gen_data.py）。
 *
 * @param seed_d Host 输入的可复现种子（uint32）
 * @param z      输出 32B，留在 UB，由 finish 写入 dk_kem 尾
 */
__aicore__ inline void DerandZFromSeedD(uint32_t seed_d, uint8_t z[32])
{
    char msg[48];
    int pos = 0;
    // 逐字节写死前缀，避免设备侧字符串字面量/库依赖
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
