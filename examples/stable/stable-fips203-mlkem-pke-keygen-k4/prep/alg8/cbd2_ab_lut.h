// @probe stable-fips203-mlkem-pke-keygen-k4
// @file prep/alg8/cbd2_ab_lut.h
// @layer prep
// @role prep/alg8：η=2 centered binomial（secret/noise）设备采样与 LUT；与 presample/alg7 链接成 prep 链。 / Alg.8 η=2 CBD prep helpers. 本文件 `cbd2_ab_lut.h` 为该子模块组件。 / Component: cbd2_ab_lut.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 1 Alg.8 CBD_η=2：ŝ/ê 采样。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：prep/alg8/cbd2_ab_lut.h
 */
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

/** 16 项 ROM：行主序 a=0..3，列 b=0..3。 */
constexpr int32_t CBD2_AB_LUT[16] = {
    0,     3328,  3327,  3326,  // a=0
    1,     0,     3328,  3327,  // a=1
    2,     1,     0,     3328,  // a=2
    3,     2,     1,     0,     // a=3
};

}  // namespace F203CbdEta2
