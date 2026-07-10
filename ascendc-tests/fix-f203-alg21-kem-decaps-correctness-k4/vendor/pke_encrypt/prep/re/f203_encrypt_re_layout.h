/**
 * @file f203_encrypt_re_layout.h
 * @brief Alg.14 G1 Launch-2：coins → r[4,256] + e1[4,256] + e2[256] 几何（ml_kem_1024）。
 */
#pragma once

#include "kernel_operator.h"
#include <cstdint>

namespace F203EncryptRe {

constexpr uint32_t kKyberK = 4U;
constexpr uint32_t kKyberN = 256U;
constexpr uint32_t kEta1 = 2U;
constexpr uint32_t kEta2 = 2U;

/** PRF 输出字节 / poly：η·N/4。ml_kem_1024 下 η₁=η₂=2 → 128B。 */
constexpr uint32_t kPrfBytesPerPoly = (kEta1 * kKyberN) / 4U;

/** Alg.14：4×r + 4×e₁ + 1×e₂ = 9 个 CBD poly；PRF nonce 0..8。 */
constexpr uint32_t kEncryptCbdPolys = 2U * kKyberK + 1U;  // 9
constexpr uint32_t kPrfBatch = kEncryptCbdPolys;

constexpr uint32_t kPrfMsgLen = 33U;  // coins[32] || byte(nonce)
constexpr uint32_t kPrfOutLen = kPrfBytesPerPoly;

constexpr uint32_t kPrfTotalBytes = kPrfBatch * kPrfOutLen;
constexpr uint32_t kRPolyCount = kKyberK;
constexpr uint32_t kE1PolyCount = kKyberK;
constexpr uint32_t kE2PolyCount = 1U;

constexpr uint32_t kRBytes = kRPolyCount * kKyberN * sizeof(int32_t);
constexpr uint32_t kE1Bytes = kE1PolyCount * kKyberN * sizeof(int32_t);
constexpr uint32_t kE2Bytes = kE2PolyCount * kKyberN * sizeof(int32_t);
constexpr uint32_t kReTotalBytes = kRBytes + kE1Bytes + kE2Bytes;

/** 扁平 re GM 中 r / e1 / e2 起始 int32 下标。 */
constexpr uint32_t kROffsetCoeffs = 0U;
constexpr uint32_t kE1OffsetCoeffs = kRPolyCount * kKyberN;
constexpr uint32_t kE2OffsetCoeffs = kE1OffsetCoeffs + kE1PolyCount * kKyberN;

/** PRF nonce → CBD 行号：0..3→r，4..7→e1，8→e2。设备侧可调用。 */
__aicore__ inline uint32_t PrfRowToReOffsetCoeffs(uint32_t row)
{
    if (row < kRPolyCount) {
        return row * kKyberN;
    }
    if (row < kRPolyCount + kE1PolyCount) {
        return kE1OffsetCoeffs + (row - kRPolyCount) * kKyberN;
    }
    return kE2OffsetCoeffs;
}

}  // namespace F203EncryptRe
