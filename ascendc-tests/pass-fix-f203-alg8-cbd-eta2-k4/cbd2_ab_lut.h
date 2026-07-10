/**
 * @file cbd2_ab_lut.h
 * @brief P1a：Alg.8 η=2 系数 LUT — (a,b) ↦ (a−b) mod 3329。
 *
 * 索引：idx = (a << 2) | b，其中 a,b ∈ {0,1,2,3} 来自 SWAR 解压后的 2-bit 半系数。
 * 预计算避免设备 hot path 上的符号分支与运行时 `% Q`（P0 每系数 2–3 条分支）。
 *
 * Golden：与 `golden_se_sampling.sample_poly_cbd2` / C `fips203_sample_poly_cbd2_row` 一致。
 * 详见 docs/notes/F203-CBD-eta2-性能优化技术总结.md §3.1。
 */
#pragma once

#include <cstdint>

namespace F203CbdEta2 {

/**
 * 16 项 ROM：行主序 a=0..3，列 b=0..3，`CBD2_AB_LUT[(a<<2)|b] = (a-b) mod 3329`。
 * 数值来源：η=2 时 CBD 系数为两个 2-bit 汉明重量之差 a-b（a,b∈[0,3]），取模 Q=3329
 * 展开为查表常量，避免设备侧运行时减法+条件加 Q+取模（P0 对照路径的做法）。
 * 例：a=0,b=1 → -1 mod 3329 = 3328（表中第 0 行第 1 列）。
 */
constexpr int32_t CBD2_AB_LUT[16] = {
    0,     3328,  3327,  3326,  // a=0
    1,     0,     3328,  3327,  // a=1
    2,     1,     0,     3328,  // a=2
    3,     2,     1,     0,     // a=3
};

}  // namespace F203CbdEta2
