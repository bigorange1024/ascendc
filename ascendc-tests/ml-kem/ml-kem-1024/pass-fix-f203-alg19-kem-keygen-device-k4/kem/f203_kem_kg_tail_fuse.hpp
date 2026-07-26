/**
 * @file f203_kem_kg_tail_fuse.hpp
 * @brief FIPS 203 Alg.16 KeyGen_internal 尾段，内嵌于 Launch-2 mmad_custom 末尾。
 *
 * 流水线位置（device-k4，2 launch）：
 *   L1 stable f203_keygen_prep  — Alg.13 行 3–15（d 在 device UB）
 *   L2 stable mmad_custom       — Alg.13 行 16–21 + 本尾段（F203_KEM_KEYGEN_TAIL=1）
 *
 * 在 FuseEkPke 已写出 ek_pke_gm / dk_pke_gm（sk_out）之后，本函数完成 Alg.16 剩余步骤：
 *   1) UB 内派生 z（DerandZFromSeedD，或旁路 A 读 kem_seed[32:64]）；
 *   2) H(ek) = SHA3-256(ek_pke)，输入 1568B，输出 32B；
 *   3) 按 liboqs 展开布局拼接 dk_kem_gm = dk_pke ‖ ek ‖ H(ek) ‖ z（3168B）。
 *
 * ek_kem 与 ek_pke_gm **同址**（Alg.16「ek ← ek_PKE」），本函数不写 ek 缓冲。
 *
 * ## 工程接线（P1，2026-07-10 定案保留）
 *
 * 尾段通过 stable `mmad_custom.cpp` 的宏 `F203_KEM_KEYGEN_TAIL` 挂接，而非 fork 整份 mmad：
 *   - 避免 stable NTT/Alg.11/行21 演进时手工 merge `mmad_custom_kem.cpp`；
 *   - stable 默认宏=0，纯 PKE 零影响；
 *   - 尾段算法仍集中在本目录 kem/*.hpp，便于 alg20/21 复用模式。
 *
 * SIM tick 相对 fork 首期约 +1.8%（~713k vs ~700k），I/O 与 correctness 3-launch 完全一致。
 *
 * ## SHA3-256 替换指南（日后第三方 AscendC 实现）
 *
 * 本文件仅 **一处** 哈希调用，须保持语义：
 *   `Sha3OneShot(h, 32, ek_ub, kEkKemBytes)` — 对 1568B ek 做 FIPS SHA3-256，输出 32B。
 *
 * 替换步骤：
 *   1) 保持函数签名与 dk_kem 拼接偏移不变（见 f203_kem_kg_layout.h）；
 *   2) 将下行 `F203SeDeviceKeccak::Sha3OneShot` 换为第三方 `__aicore__` 一次性摘要 API；
 *   3) 验收：`bash run.sh -r cpu/sim` + `cmp` correctness output；可选对比 H(ek) 中间量。
 *
 * 当前实现：`library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp`（标量 Keccak-f[1600]）。
 * z 的 SHA3-256 在 `f203_kem_kg_derand_ub.hpp`（第二处替换点，须同步更换或统一封装）。
 */
#pragma once

#include "f203_kem_kg_derand_ub.hpp"
#include "f203_kem_kg_layout.h"

namespace F203KemKg {

/** GM→GM 字节拷贝（尾段数据量 ≤3KB，标量循环即可；无第三方依赖）。 */
__aicore__ inline void GmMemcpyU8(__gm__ uint8_t *dst, __gm__ const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = src[i];
    }
}

/**
 * Launch-2 内嵌尾：Alg.19 z + H(ek) + dk_kem 拼接（仅 AIV0 调用）。
 *
 * @param seed_d_gm  GM：生产 4B seed_d LE；旁路 A（KEM_KG_EXT_SEED）为 64B kem_seed
 * @param ek_pke_gm  GM：1568B ek_PKE，亦为 ek_kem 别名（只读）
 * @param dk_pke_gm  GM：1536B PKE 私钥（= mmad sk_out / ByteEncode₁₂(ŝ)）
 * @param dk_kem_gm  GM：3168B 输出，liboqs 展开布局
 */
__aicore__ inline void KemKgTailFused(__gm__ uint8_t *seed_d_gm, __gm__ uint8_t *ek_pke_gm, __gm__ uint8_t *dk_pke_gm,
                                      __gm__ uint8_t *dk_kem_gm)
{
    // —— 步骤 1：z[32] 留在 UB（生产路径由 seed_d 域分离派生）——
    uint8_t z[kZBytes];
#if KEM_KG_EXT_SEED
    // 旁路 A：host 直接提供 d‖z，此处只取 z 段，跳过 DerandZFromSeedD
    const __gm__ uint8_t *seedBytes = reinterpret_cast<const __gm__ uint8_t *>(seed_d_gm);
    for (uint32_t i = 0; i < kZBytes; ++i) {
        z[i] = seedBytes[32U + i];
    }
#else
    const uint32_t seed_d = *reinterpret_cast<__gm__ uint32_t *>(seed_d_gm);
    DerandZFromSeedD(seed_d, z);
#endif

    // —— 步骤 2：H(ek) = SHA3-256(ek_pke[1568]) → h[32] ——
    // ek 先拷入 UB：当前 Sha3OneShot 接口消费 uint8_t* 而非 __gm__*（替换第三方时留意）
    uint8_t ek_ub[kEkKemBytes];
    for (uint32_t i = 0; i < kEkKemBytes; ++i) {
        ek_ub[i] = ek_pke_gm[i];
    }
    uint8_t h[kHashEkBytes];
    // 【SHA3 替换点 #2】mdlen=32 → SHA3-256；输入长 kEkKemBytes=1568
    F203SeDeviceKeccak::Sha3OneShot(h, static_cast<int>(kHashEkBytes), ek_ub, kEkKemBytes);

    // —— 步骤 3：dk_kem = dk_pke ‖ ek ‖ H(ek) ‖ z（偏移见 f203_kem_kg_layout.h）——
    GmMemcpyU8(dk_kem_gm, dk_pke_gm, kDkPkeBytes);
    GmMemcpyU8(dk_kem_gm + kDkKemOffEk, ek_pke_gm, kEkKemBytes);
    for (uint32_t i = 0; i < kHashEkBytes; ++i) {
        dk_kem_gm[kDkKemOffHashEk + i] = h[i];
    }
    for (uint32_t i = 0; i < kZBytes; ++i) {
        dk_kem_gm[kDkKemOffZ + i] = z[i];
    }
}

}  // namespace F203KemKg
