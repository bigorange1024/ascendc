/**
 * @file f203_kem_enc_init.hpp
 * @brief FIPS 203 Alg.17 Encaps_internal 设备头：读外部 m，算 H(ek)/G(m‖h)，写 K 与 r。
 *
 * 流水线位置：并入 `f203_kem_enc_prep` 入口前段（**不**另开 launch）；其后同一核继续 Encrypt prep。
 * 对齐：`stable-fips203-mlkem-kem-encaps-k4-实现方案-customspec.*` §Alg.17；
 * 基线行为同 `pass-fix-f203-alg20-kem-encaps-device-k4`。
 *
 * 密码学（FIPS 203）：
 *   h ← H(ek)           = SHA3-256(ek[1568])
 *   (K ‖ r) ← G(m ‖ h)  = SHA3-512(64B 输入)
 *   随后 Encrypt 消费 r（作 Alg.14 随机性），产出 c；K 为共享秘密。
 *
 * 背景 / 结论 / 未采用：
 *   - 背景：Alg.17 要求 m 为输入，H/G 在设备侧；历史 correctness 曾 Host 预填 r，已否决。
 *   - 结论：仅 block0（或 CPU 串行入口）调用本函数；r/K 写入 GM workspace / 输出缓冲。
 *   - 未采用：为 H/G 独立第 3 launch；Host 伪 H/G；把 SHA3hp 未评估路径抄进默认。
 *
 * SHA3：`library/shared` → `F203SeDeviceKeccak::Sha3OneShot`（当前标量；日后可换后端，勿改切分）。
 */
#pragma once

#include "f203_kem_enc_layout.h"
#include "fips203_device_sha3.hpp"

namespace F203KemEnc {

/**
 * UB→GM 逐字节写（标量路径）。
 * @param dst GM 目标
 * @param src UB/栈上源缓冲
 * @param n   字节数；本头段落点数据 ≤64B（K 或 r 各 32B）
 * 说明：短块；customspec 允许日后改为小块 DataCopy，非正确性门槛。
 */
__aicore__ inline void WriteGmBytes(__gm__ uint8_t *dst, const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

/**
 * Encaps_internal 头段（仅 AIV0 / block0，或 CPU 串行入口调用）。
 *
 * @param ek_gm  1568B 封装公钥（= ek_PKE）；只读
 * @param m_gm   输入 32B 消息 m（外部读入；生产勿导出中间哈希）
 * @param K_gm   输出共享秘密 K（32B）→ 最终 D2H 为 output/K.bin
 * @param r_gm   输出 r=G 后半（32B）→ 喂 Encrypt prep 的 PRF+CBD；禁止 Host 预填
 *
 * 前置：调用方保证本核负责写 K/r；其它核不得并发写同一 GM。
 * 后置：r_gm 已可供 BuildEncryptPrepSinglePipe 使用；K_gm 在全链结束前保持。
 */
__aicore__ inline void KemEncInitHead(__gm__ uint8_t *ek_gm, __gm__ uint8_t *m_gm, __gm__ uint8_t *K_gm,
                                      __gm__ uint8_t *r_gm)
{
    // —— ① 读外部 m[32]（Alg.17 输入；逐字节标量 GM→UB）——
    uint8_t m[kHashEkBytes];
    for (uint32_t i = 0U; i < kHashEkBytes; ++i) {
        m[i] = m_gm[i];
    }

    // —— ② h = H(ek) = SHA3-256(ek[1568]) ——
    // 先把 ek 搬进 UB：当前实现为标量环；相对后续 12× Keccak-f，拷贝非主热点。
    uint8_t ek_ub[kEkKemBytes];
    for (uint32_t i = 0U; i < kEkKemBytes; ++i) {
        ek_ub[i] = ek_gm[i];
    }
    uint8_t h[kHashEkBytes];
    F203SeDeviceKeccak::Sha3OneShot(h, static_cast<int>(kHashEkBytes), ek_ub, kEkKemBytes);

    // —— ③ (K‖r) = G(m‖h) = SHA3-512（输入 64B → 输出 64B）——
    uint8_t mh[64];
    for (uint32_t i = 0U; i < kHashEkBytes; ++i) {
        mh[i] = m[i];
        mh[kHashEkBytes + i] = h[i];
    }
    uint8_t kr[kGOutBytes];
    F203SeDeviceKeccak::Sha3OneShot(kr, 64, mh, 64);

    // —— ④ 落盘到 GM：前 32B → K，后 32B → r ——
    WriteGmBytes(K_gm, kr, kSharedSecretBytes);
    WriteGmBytes(r_gm, kr + kSharedSecretBytes, kRBytes);
}

}  // namespace F203KemEnc
