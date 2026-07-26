/**
 * @file f203_encrypt_prep_layout.h
 * @brief Alg.14 Encrypt prep 单内核 I/O 尺寸（ML-KEM-768，行 3–15：Â + r/e₁/e₂）。
 *
 * 流水线位置：
 *   - 行 3–7：ek_pke 尾 ρ[32] → SampleNTT → a_hat[9,256] int32
 *   - 行 8–15：coins[32] → PRF(η=2) → CBD → re[7,256] int32（r‖e₁‖e₂）
 *
 * 与 golden I/O：
 *   - input：ek_pke.bin（kEkPkeBytes）、coins.bin（kCoinsBytes）
 *   - output：a_hat.bin（kAHatBytes）、re.bin（kReBytes）
 *   - 中间：prf_out[kPrfBatch, kPrfBytesPerPoly] 仅设备侧，不对拍落盘
 *
 * 编译开关：F203_AHAT16_BLOCK_DIM（默认 2）控制双 AIV 5+4 分片 Â；见 f203_a_hat16_config.h。
 */
#pragma once

#include <cstdint>

#ifndef F203_AHAT16_BLOCK_DIM
#define F203_AHAT16_BLOCK_DIM 2
#endif

namespace F203EncryptPrep {

/** ML-KEM-768：k=3，N=256。 */
constexpr uint32_t kKyberK = 3U;
constexpr uint32_t kKyberN = 256U;
/** Â 矩阵 poly 数：k×k = 9；禁止补到 16。 */
constexpr uint32_t kAHatPolys = kKyberK * kKyberK;
/** FIPS 203 ek_PKE 编码长度（含尾部 ρ）。 */
constexpr uint32_t kEkPkeBytes = 1184U;
/** ρ 在 ek_pke 中的字节偏移（ByteEncode₁₂(t̂)‖ρ）。 */
constexpr uint32_t kRhoOffset = 1152U;
constexpr uint32_t kRhoBytes = 32U;
/** Encrypt 随机性 coins（Alg.14 输入）。 */
constexpr uint32_t kCoinsBytes = 32U;

/** CBD η=2；PRF 每 poly 输出 (η·N)/4 = 128B。 */
constexpr uint32_t kEta = 2U;
/** re 行数：r(k) + e₁(k) + e₂(1) = 7；禁止按 k4 batch9 处理。 */
constexpr uint32_t kRePolys = 2U * kKyberK + 1U;  // 7
constexpr uint32_t kPrfBytesPerPoly = (kEta * kKyberN) / 4U;  // 128
constexpr uint32_t kPrfBatch = kRePolys;
/** PRF 消息有效长：coins[32] ‖ byte(nonce)。 */
constexpr uint32_t kPrfMsgLen = 33U;  // coins[32] || byte(nonce)

constexpr size_t kEkBytes = static_cast<size_t>(kEkPkeBytes);
constexpr size_t kCoinsSize = static_cast<size_t>(kCoinsBytes);
/** a_hat 扁平字节数：9×256×sizeof(int32)。 */
constexpr size_t kAHatBytes = static_cast<size_t>(kAHatPolys) * kKyberN * sizeof(int32_t);
/** PRF 中间态总字节：7×128。 */
constexpr size_t kPrfBytes = static_cast<size_t>(kPrfBatch) * kPrfBytesPerPoly;
/** re 扁平字节数：7×256×sizeof(int32)。 */
constexpr size_t kReBytes = static_cast<size_t>(kRePolys) * kKyberN * sizeof(int32_t);

/** prep launch blockDim（与 Â 分片数一致）。 */
constexpr uint32_t kPrepBlockDim = static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM);

}  // namespace F203EncryptPrep
