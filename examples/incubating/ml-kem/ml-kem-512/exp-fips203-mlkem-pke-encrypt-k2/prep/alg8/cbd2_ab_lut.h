// @probe exp-fips203-mlkem-pke-encrypt-k2
// @file prep/alg8/cbd2_ab_lut.h
// @layer prep
// @role prep/alg8：η=2 centered binomial（secret/noise）设备采样与 LUT；与 presample/alg7 链接成 prep 链。 / Alg.8 η=2 CBD prep helpers. 本文件 `cbd2_ab_lut.h` 为该子模块组件。 / Component: cbd2_ab_lut.h.
// @production_io D14 默认 I/O：input/ ek_pke.bin(800B)+m.bin+coins.bin；output/c.bin(768B)；中间 GM 不落盘。
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file cbd2_ab_lut.h
 * @brief P1a：Alg.8 η=2 系数 LUT — (a,b) ↦ (a−b) mod 3329。
 *
 * 索引：idx = (a << 2) | b，其中 a,b ∈ {0,1,2,3} 来自 SWAR 解压后的 2-bit 半系数。
 * 预计算避免设备 hot path 上的符号分支与运行时 `% Q`（P0 每系数 2–3 条分支）。
 *
 * Golden：与 `golden_se_sampling.sample_poly_cbd2` / C `fips203_sample_poly_cbd2_row` 一致。
 * 详见 docs/notes/F203-CBD-eta2-性能优化技术总结.md §3.1。
 * Encrypt prep：f203_encrypt_re_cbd / SamplePolyCbd2RowSwLutUb 查本表；巨表仅文件头说明用途。
 */
#pragma once

#include <cstdint>

namespace F203CbdEta2 {

/** 16 项 ROM：行主序 a=0..3，列 b=0..3。 */
constexpr int32_t CBD2_AB_LUT[16] = {
    0,     3328,  3327,  3326,  // a=0
    1,     0,     3328,  3327,  // a=1
    2,     1,     0,     3328,  // a=2
    3,     2,     1,     0,     // a=3
};

}  // namespace F203CbdEta2
