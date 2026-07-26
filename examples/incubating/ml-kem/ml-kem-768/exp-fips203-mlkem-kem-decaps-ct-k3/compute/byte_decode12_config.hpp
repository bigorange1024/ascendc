#pragma once

/**
 * @file byte_decode12_config.hpp
 * @brief 行 2 ByteDecode₁₂ 实现选型（CMake：`F203_BYTE_DECODE12_IMPL`）。
 *
 * ## 两条路径（均已验证 I/O；默认走 tick 更低者）
 *
 * | 值 | 名称 | 实现 | 角色 |
 * |----|------|------|------|
 * | **0** | 标量 | `poly_byte_decode12_scalar_gm` | **生产默认 / 继续推进**；单循环 128 pair；SIM ~113k |
 * | **1** | 零 Gather 向量 | `poly_byte_decode12_alg7_gm` | **备用**；见下 |
 *
 * ## IMPL=1 是「零 Gather」而非 Gather 路线
 *
 * 曾试验 **Gather 解交织**（expanded + 每 poly 3×Gather）→ tick ~145k，**已废弃**。
 * IMPL=1 对齐 Alg7 生产定稿（`F203_ALG7_D12_GATHER=0`）：
 *
 *   DataCopy(384B) → UB
 *   → 标量解交织 c0/c1/c2[128]（GetValue，**不用 Gather**）
 *   → 向量 compute 128 lane 一次（MaskLowBits + ShiftRight/Muls/Add）
 *   → 标量交织写 t_hat[256]
 *
 * 共享实现：`library/shared/f203_byte_codec/byte_decode12_vec.hpp`
 *
 * ## 切换方式（须显式，非默认）
 *
 *   F203_BYTE_DECODE12_IMPL=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
 */
#ifndef F203_BYTE_DECODE12_IMPL
#define F203_BYTE_DECODE12_IMPL 0
#endif
