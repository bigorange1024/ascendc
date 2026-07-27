/**
 * @file cbd2_ab_lut.h
 * @brief Alg.8 η=2 系数 LUT — (a,b) ↦ (a−b) mod 3329。
 *
 * 本文件在设备流水线中的位置：`f203_cbd_eta2_sw_lut.hpp` 的热路径查表常量。
 * 索引为 `idx=(a<<2)|b`，其中 a,b 是 η=2 CBD 的两个 2-bit 汉明重量。
 * Golden：与 `golden_se_sampling.sample_poly_cbd2` 同语义；设备侧只要求 I/O 等价。
 */
#pragma once

#include <cstdint>

namespace F203CbdEta2 {

/**
 * 16 项 ROM：行主序 a=0..3，列 b=0..3，`CBD2_AB_LUT[(a<<2)|b] = (a-b) mod 3329`。
 * 负值按 FIPS 203 模数 q=3329 映射到 [0,q)，避免 hot path 每系数条件加 q 与取模。
 */
constexpr int32_t CBD2_AB_LUT[16] = {
    0,     3328,  3327,  3326,  // a=0
    1,     0,     3328,  3327,  // a=1
    2,     1,     0,     3328,  // a=2
    3,     2,     1,     0,     // a=3
};

}  // namespace F203CbdEta2
