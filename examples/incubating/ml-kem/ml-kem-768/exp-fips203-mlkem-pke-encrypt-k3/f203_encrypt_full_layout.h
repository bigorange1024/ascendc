/**
 * @file f203_encrypt_full_layout.h
 * @brief Alg.14 完整 Encrypt（FIPS 行 1–22；prep 行 3–15 + compute 行 2/16–22）prep→compute 的 GM handoff 契约。
 *
 * 数据面（单 device arena，禁止 D2H 中转）：
 *   prep 写 a_hat[9,256] int32 + re[7,256] int32；compute 零拷贝读同一 GM。
 *   re 扁平布局：poly 0–2 = r(≡y)，3–5 = e₁，6 = e₂（与 prep golden_re 一致）。
 *
 * 本头把 prep 侧尺寸（F203EncryptPrep::*）与 compute 侧尺寸（tiling::*）用
 * static_assert 绑定，任何一侧改形状都会在编译期暴露，避免 handoff 静默错位。
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "f203_encrypt_prep_layout.h"  // F203EncryptPrep::*
#include "f203_l18_l19_tiling.h"       // tiling::*

namespace F203EncryptFull {

/** 单 poly 系数个数（k=3，n=256）。 */
constexpr size_t kN = F203EncryptPrep::kKyberN;

/** re[7,256] → compute 输入的字节偏移（int32 元素 × 4B）。 */
constexpr size_t kReYByteOff = 0;                        /**< r ≡ y：poly 0–2 */
constexpr size_t kReE1ByteOff = 3U * kN * sizeof(int32_t); /**< e₁：poly 3–5（3072B） */
constexpr size_t kReE2ByteOff = 6U * kN * sizeof(int32_t); /**< e₂：poly 6（6144B） */

// ---- 编译期 handoff 尺寸一致性（prep 输出 == compute 输入）----
static_assert(F203EncryptPrep::kAHatBytes == tiling::aHatFileBytes, "a_hat handoff 尺寸不一致");
static_assert(3U * kN * sizeof(int32_t) == tiling::yFileBytes, "y(=r) 切片尺寸不一致");
static_assert(3U * kN * sizeof(int32_t) == tiling::e1FileBytes, "e₁ 切片尺寸不一致");
static_assert(kN * sizeof(int32_t) == tiling::e2FileBytes, "e₂ 切片尺寸不一致");
static_assert(F203EncryptPrep::kReBytes == 7U * kN * sizeof(int32_t), "re 总尺寸应为 7*256*4");

}  // namespace F203EncryptFull
