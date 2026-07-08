/**
 * @file f203_encrypt_full_layout.h
 * @brief Alg.14 全链 Encrypt（行 3–24）prep→compute 的 GM handoff 契约。
 *
 * 数据面（单 device arena，禁止 D2H 中转）：
 *   prep 写 a_hat[16,256] int32 + re[9,256] int32；compute 零拷贝读同一 GM。
 *   re 扁平布局：poly 0–3 = r(≡y)，4–7 = e₁，8 = e₂（与 prep golden_re 一致）。
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

/** 单 poly 系数个数（k=4，n=256）。 */
constexpr size_t kN = F203EncryptPrep::kKyberN;

/** re[9,256] → compute 输入的字节偏移（int32 元素 × 4B）。 */
constexpr size_t kReYByteOff = 0;                        /**< r ≡ y：poly 0–3 */
constexpr size_t kReE1ByteOff = 4U * kN * sizeof(int32_t); /**< e₁：poly 4–7（4096B） */
constexpr size_t kReE2ByteOff = 8U * kN * sizeof(int32_t); /**< e₂：poly 8（8192B） */

// ---- 编译期 handoff 尺寸一致性（prep 输出 == compute 输入）----
static_assert(F203EncryptPrep::kAHatBytes == tiling::aHatFileBytes, "a_hat handoff 尺寸不一致");
static_assert(4U * kN * sizeof(int32_t) == tiling::yFileBytes, "y(=r) 切片尺寸不一致");
static_assert(4U * kN * sizeof(int32_t) == tiling::e1FileBytes, "e₁ 切片尺寸不一致");
static_assert(kN * sizeof(int32_t) == tiling::e2FileBytes, "e₂ 切片尺寸不一致");
static_assert(F203EncryptPrep::kReBytes == 9U * kN * sizeof(int32_t), "re 总尺寸应为 9*256*4");

}  // namespace F203EncryptFull
