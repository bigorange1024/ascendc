#pragma once

/**
 * @file f203_encrypt_at_jp_mod.hpp
 * @brief Alg.14 行 18 内积累加后的 final mod q：转发 library/shared/f203_mod_q。
 *
 * 流水线：EncryptAtJpHalfRowsVec 在 Σ_j MultiplyNTTs 之后调用 mod_q_*_vec。
 * 本文件无独立算法，仅导出 Barrett 常量与向量入口别名。
 */
#include "f203_encrypt_at_jp_tiling.h"
#include "f203_mod_q/mod_q_vec.hpp"

namespace encrypt_at_jp {

constexpr int32_t kBarrettMu = f203_mod_q::kBarrettMu;
constexpr int32_t kBarrettK = f203_mod_q::kBarrettK;

using f203_mod_q::mod_q_barrett_vec;
using f203_mod_q::mod_q_final_vec;

} // namespace encrypt_at_jp
