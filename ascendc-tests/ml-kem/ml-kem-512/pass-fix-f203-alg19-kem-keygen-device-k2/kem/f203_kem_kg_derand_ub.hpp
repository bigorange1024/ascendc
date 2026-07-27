/**
 * @file f203_kem_kg_derand_ub.hpp
 * @brief FIPS 203 Alg.19 随机性 z：在 device UB 内用 SHA3-256 做域分离派生。
 *
 * 流水线分工（device-k2，2 launch）：
 *   - d（KeyGen 主随机性）由 L1 D13 k2 prep 内 DerandFromSeedD / HashG 消费，不在本文件；
 *   - z 由本文件在 L2 尾段 KemKgTailFused 内派生，写入 dk_kem 尾 32B。
 *
 * 背景（已锁定）：d/z 均在 AscendC 生成，禁止 D2H/落盘随机性本体；
 * Host 可复现验收只给 seed_d.bin（4B）。旁路 A（KEM_KG_EXT_SEED）不走本函数。
 *
 * ## SHA3-256 替换指南（日后第三方 AscendC 实现）
 *
 * 本文件 **一处** 哈希调用：
 *   `Sha3OneShot(z, 32, msg, pos)` — 对域分离消息做 SHA3-256，输出 32B z。
 *
 * 消息格式（与 golden 锁定，不可改）：
 *   `"exp-mlkem-f203-kem-k2:SEED_Z=" ‖ decimal(seed_d)`（见 scripts/gen_data.py）。
 *
 * 替换步骤：
 *   1) 保持消息拼接与 U32ToDec 不变，或 golden 同步改；
 *   2) 替换 `F203SeDeviceKeccak::Sha3OneShot` 为第三方 API（须 `__aicore__`、一次性摘要）；
 *   3) 与 `f203_kem_kg_tail_fuse.hpp` 中 H(ek) 的 SHA3 可统一封装为 `kem/f203_kem_sha3.hpp`。
 *
 * 当前实现：`library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp`。
 * D13 k2 prep 内 SHA3-512（ρ‖σ）走同库不同 mdlen，替换第三方时 prep 与 kem 可一并评估。
 */
#pragma once

#include "fips203_device_sha3.hpp"

namespace F203KemKg {

/**
 * 将 uint32 写成十进制 ASCII（无堆分配；与 D13 k2 Alg.7 DerandFromSeedD 同型）。
 * @param v   待转换无符号整数（SEED_D）
 * @param out 输出缓冲，调用方保证 ≥10 字节
 * @return 写入的字符数（不含 '\\0'）
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
 * 由 SEED_D 派生 Alg.19 的 z[32]（留在 UB，由 KemKgTailFused 写入 dk_kem 尾）。
 *
 * 消息 = 固定前缀 "exp-mlkem-f203-kem-k2:SEED_Z=" ‖ 十进制(seed_d)，
 * 再 SHA3-256 → 32B。前缀与 host golden / liboqs fixture 对齐（见 scripts/gen_data.py）。
 *
 * @param seed_d Host 输入的可复现种子（uint32）
 * @param z      输出 32B，留在 UB
 */
__aicore__ inline void DerandZFromSeedD(uint32_t seed_d, uint8_t z[32])
{
    char msg[48];
    int pos = 0;
    // 逐字节写死前缀，避免设备侧对 C 字符串字面量 / 标准库的依赖
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
    msg[pos++] = '2';
    msg[pos++] = ':';
    msg[pos++] = 'S';
    msg[pos++] = 'E';
    msg[pos++] = 'E';
    msg[pos++] = 'D';
    msg[pos++] = '_';
    msg[pos++] = 'Z';
    msg[pos++] = '=';
    pos += U32ToDec(seed_d, msg + pos);
    // 【SHA3 替换点 #1】mdlen=32 → SHA3-256；输入长 pos（典型 ≤40B）
    F203SeDeviceKeccak::Sha3OneShot(z, 32, reinterpret_cast<const uint8_t *>(msg), static_cast<uint32_t>(pos));
}

}  // namespace F203KemKg
