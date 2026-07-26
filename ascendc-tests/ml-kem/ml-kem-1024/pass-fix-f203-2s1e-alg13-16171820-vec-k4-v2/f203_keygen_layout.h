/**
 * @file f203_keygen_layout.h
 * @brief Alg.13 KeyGen 行 21 I/O 尺寸常量（k=4）。
 *
 * 流水线位置：F203_KEYGEN_EK_PKE=1 时 main / FuseEkPke / gen_data 共用。
 * 作用：ek_polyvec ‖ ρ → ek_PKE 的字节长度；与 ByteEncode₁₂ polyvec 尺寸衔接。
 * 与 golden 关系：output/ek_pke.bin 须为 kEkPkeBytes（1568）；ρ 输入 32B。
 *
 * 注：注释中「ML-KEM-768」易与 k=3 混淆；本探针 k=4（ML-KEM-1024 参数集维数）。
 */
#pragma once

#include <cstdint>

namespace F203Keygen {

constexpr uint32_t kKyberK = 4U;                         /**< polyvec 维数 k */
constexpr uint32_t kPolyBytesEncoded = 384U;             /**< ByteEncode₁₂ 单 poly */
constexpr uint32_t kEkPolyvecBytes = kKyberK * kPolyBytesEncoded;  /**< 1536：ek 编码体 */
constexpr uint32_t kRhoBytes = 32U;                      /**< 公钥种子 ρ */
constexpr uint32_t kEkPkeBytes = kEkPolyvecBytes + kRhoBytes;      /**< 1568：ek_PKE */
constexpr uint32_t kDkPkeBytes = kEkPolyvecBytes;                  /**< 1536：dk 侧同宽占位 */

}  // namespace F203Keygen
