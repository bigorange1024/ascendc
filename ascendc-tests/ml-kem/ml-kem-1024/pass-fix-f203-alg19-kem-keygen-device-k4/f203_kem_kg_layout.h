/**
 * @file f203_kem_kg_layout.h
 * @brief FIPS 203 Alg.19（ML-KEM.KeyGen）I/O 尺寸与 dk_kem 内存布局常量（ml_kem_1024 / k=4）。
 *
 * 本探针流水线（device-k4，2 launch）：
 *   Host 仅喂 seed_d + LUT → stable PKE prep + mmad（F203_KEM_KEYGEN_TAIL=1 内嵌 Alg.16 尾）
 *   → output/ek_kem.bin（=ek_pke 别名）、dk_kem.bin。
 *
 * dk_kem 采用 **liboqs 展开布局**（非 FIPS 紧凑 ByteEncode 串联）：
 *
 *   偏移 0      : dk_pke[1536]   — ByteEncode₁₂(ŝ)，来自 mmad sk_out
 *   偏移 1536   : ek[1568]       — ek_PKE 别名（与 ek_kem.bin 同内容）
 *   偏移 3104   : H(ek)[32]      — SHA3-256(ek)，设备尾段计算
 *   偏移 3136   : z[32]          — Alg.19 隐式拒绝秘密，设备 UB 派生
 *   总长 3168B
 *
 * golden / liboqs 对拍须按上表切片；禁止与 vendor PKE layout 混用。
 *
 * SHA3 替换时禁止改动上表偏移与字节数；仅替换 kem 目录内哈希原语实现。
 */
#pragma once

#include <cstdint>

namespace F203KemKg {

/** 生产路径：Host 输入 seed_d.bin 为 4B uint32 LE。 */
constexpr uint32_t kSeedBytes = 4U;
/** 旁路 A（KEM_KG_EXT_SEED，test-only）：host kem_seed = d(32)‖z(32)。 */
constexpr uint32_t kExtSeedBytes = 64U;
/** Alg.13 PKE 私钥字节数（ByteEncode₁₂(ŝ)×k）。 */
constexpr uint32_t kDkPkeBytes = 1536U;
/** KEM/PKE 公钥字节数（ek_PKE = ByteEncode₁₂(t̂)‖ρ）。 */
constexpr uint32_t kEkKemBytes = 1568U;
/** H(ek) = SHA3-256 摘要长度（FIPS 203 / liboqs 一致）。 */
constexpr uint32_t kHashEkBytes = 32U;
/** Alg.19 隐式拒绝秘密 z 长度。 */
constexpr uint32_t kZBytes = 32U;
/** liboqs 展开 dk_kem 总长：dk_pke ‖ ek ‖ H(ek) ‖ z */
constexpr uint32_t kDkKemBytes = kDkPkeBytes + kEkKemBytes + kHashEkBytes + kZBytes;  // 3168

/** dk_kem 内各段起始偏移（拼接时供 KemKgTailFused 使用）。 */
constexpr uint32_t kDkKemOffEk = kDkPkeBytes;
constexpr uint32_t kDkKemOffHashEk = kDkPkeBytes + kEkKemBytes;
constexpr uint32_t kDkKemOffZ = kDkPkeBytes + kEkKemBytes + kHashEkBytes;

}  // namespace F203KemKg
