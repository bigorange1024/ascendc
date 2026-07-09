/**
 * @file f203_encrypt_t_dot_r_layout.h
 * @brief G3 L5：t̂·r̂ 单 poly 内积 GM 偏移。
 *
 * t̂[j] @ t_hat[j*N+c]；r̂[j] @ r_hat[j*N+c]；输出 tr_hat[c]（256 int32）。
 */
#pragma once

/** t̂[j] / r̂[j] 偏移；kN 与 innerproduct_tiling 一致（256）。 */
namespace f203_t_dot_r_layout {

constexpr int32_t kK = 4;
constexpr int32_t kN = 256;

__aicore__ inline uint32_t polyvec_offset(int32_t j)
{
    return static_cast<uint32_t>(j) * static_cast<uint32_t>(kN);
}

constexpr int32_t kTrHatBytes = kN * static_cast<int32_t>(sizeof(int32_t));

} // namespace f203_t_dot_r_layout
