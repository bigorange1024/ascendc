#pragma once

/**
 * @file f203_encrypt_at_jp_mod.hpp
 * @brief 行 19 内积 final mod：复用 library/shared/f203_mod_q。
 */
#include "f203_encrypt_at_jp_tiling.h"
#include "f203_mod_q/mod_q_vec.hpp"

namespace encrypt_at_jp {

constexpr int32_t kBarrettMu = f203_mod_q::kBarrettMu;
constexpr int32_t kBarrettK = f203_mod_q::kBarrettK;

using f203_mod_q::mod_q_barrett_vec;
using f203_mod_q::mod_q_final_vec;

} // namespace encrypt_at_jp
