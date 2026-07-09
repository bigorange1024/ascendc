/**
 * @file f203_kem_kg_layout.h
 * @brief FIPS 203 Alg.19（ML-KEM.KeyGen）I/O 尺寸常量：ml_kem_1024 / k=4。
 *
 * 本探针流水线位置：
 *   Host 仅喂 seed_d（或旁路 A 的 kem_seed）→ vendor PKE KeyGen（Alg.13 两 launch）
 *   → 本仓 kem/ 尾段（H(ek)+UB 内 z+拼接）→ output/ek_kem.bin、dk_kem.bin。
 *
 * dk_kem 采用 liboqs 展开布局（非 FIPS 紧凑编码）：
 *   dk_pke[1536] ‖ ek[1568] ‖ H(ek)[32] ‖ z[32] = 3168B。
 * 与 golden / liboqs fixture 对拍时须按此切片；禁止把本头当作 vendor PKE layout 的替代。
 */
#pragma once

#include <cstdint>

namespace F203KemKg {

/** 生产路径：Host 输入 seed_d.bin 为 4B uint32 LE。 */
constexpr uint32_t kSeedBytes = 4U;
// 旁路 A（KEM_KG_EXT_SEED，test-only）：host 直接提供的 kem_seed 长度 = d(32)‖z(32)。
// 生产路径（宏关）仍只吃 4B seed_d，由 device UB 派生 d/z；本常量仅供测试构建扩 seed GM 缓冲。
constexpr uint32_t kExtSeedBytes = 64U;
/** Alg.13 PKE 私钥字节数（ByteEncode₁₂(ŝ)×k）。 */
constexpr uint32_t kDkPkeBytes = 1536U;
/** KEM/PKE 公钥字节数（ek_PKE = ByteEncode₁₂(t̂)‖ρ）。 */
constexpr uint32_t kEkKemBytes = 1568U;
/** H(ek) = SHA3-256 摘要长度。 */
constexpr uint32_t kHashEkBytes = 32U;
/** Alg.19 隐式拒绝秘密 z 长度（KeyGen_internal 写入 dk 尾）。 */
constexpr uint32_t kZBytes = 32U;
/** liboqs 展开：dk_pke ‖ ek ‖ H(ek) ‖ z */
constexpr uint32_t kDkKemBytes = kDkPkeBytes + kEkKemBytes + kHashEkBytes + kZBytes;  // 3168

}  // namespace F203KemKg
