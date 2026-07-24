#pragma once

/**
 * @file f203_encrypt_at_jp_mod.hpp
 * @brief Alg.14 行 18/19 内积最终 mod q：转发 library/shared/f203_mod_q 向量实现。
 *
 * 流水线位置：Encrypt compute 内积段（Â·ŷ / t̂·ŷ）累加后归约到 [0,q)。
 * 本头无独立算法，仅把 Barrett 常量与 `mod_q_*_vec` 导入 `encrypt_at_jp` 命名空间。
 */
#include "f203_encrypt_at_jp_tiling.h"
#include "f203_mod_q/mod_q_vec.hpp"

namespace encrypt_at_jp {

constexpr int32_t kBarrettMu = f203_mod_q::kBarrettMu;
constexpr int32_t kBarrettK = f203_mod_q::kBarrettK;

using f203_mod_q::mod_q_barrett_vec;
using f203_mod_q::mod_q_final_vec;

} // namespace encrypt_at_jp
