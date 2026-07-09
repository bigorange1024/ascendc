/**
 * @file f203_encrypt_prep_layout.h
 * @brief Alg.14 Encrypt prep 单内核 I/O 尺寸常量（行 3–15：Â + r/e₁/e₂）。
 *
 * 流水线位置：FIPS 203 / ML-KEM-1024（k=4）K-PKE.Encrypt 的 **prep 段**；
 * host（`main.cpp`）与设备（`f203_encrypt_prep*`）共用本头中的字节/元素数。
 * 与 golden：描述 `input/ek_pke`、`coins` 及设备中间 Â/re 的几何，不改变 `c.bin` 语义。
 */
#pragma once

#include <cstdint>

#ifndef F203_AHAT16_BLOCK_DIM
/** Â 双 AIV 分片：block0→poly 0–7，block1→8–15。 */
#define F203_AHAT16_BLOCK_DIM 2
#endif

namespace F203EncryptPrep {

constexpr uint32_t kKyberK = 4U;       // ML-KEM-1024 的 k
constexpr uint32_t kKyberN = 256U;     // 每 poly 系数个数
constexpr uint32_t kAHatPolys = kKyberK * kKyberK;  // Â 共 16 poly
constexpr uint32_t kEkPkeBytes = 1568U;             // ByteEncode₁₂(t̂)‖ρ
constexpr uint32_t kRhoOffset = 1536U;              // ρ 在 ek 中的字节偏移
constexpr uint32_t kRhoBytes = 32U;
constexpr uint32_t kCoinsBytes = 32U;               // Alg.14 随机性 coins

constexpr uint32_t kEta = 2U;                       // CBD η=2
constexpr uint32_t kRePolys = 2U * kKyberK + 1U;    // r(4)+e₁(4)+e₂(1)=9
constexpr uint32_t kPrfBytesPerPoly = (kEta * kKyberN) / 4U;  // 128B/行
constexpr uint32_t kPrfBatch = kRePolys;
constexpr uint32_t kPrfMsgLen = 33U;  // coins[32] || byte(nonce)

constexpr size_t kEkBytes = static_cast<size_t>(kEkPkeBytes);
constexpr size_t kCoinsSize = static_cast<size_t>(kCoinsBytes);
constexpr size_t kAHatBytes = static_cast<size_t>(kAHatPolys) * kKyberN * sizeof(int32_t);
constexpr size_t kPrfBytes = static_cast<size_t>(kPrfBatch) * kPrfBytesPerPoly;
constexpr size_t kReBytes = static_cast<size_t>(kRePolys) * kKyberN * sizeof(int32_t);

constexpr uint32_t kPrepBlockDim = static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM);

}  // namespace F203EncryptPrep
