/**
 * @file cbd2_ab_lut.h
 * @brief CBD η=2 的 a/b LUT（脚本或手填常量表）。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-1024 Encrypt prep 行 8–15。
 * 与 golden：与 host sample_poly_cbd2 I/O 等价。
 */

// @probe stable-fips203-mlkem-pke-keygen-k4
// @file prep/alg8/cbd2_ab_lut.h
// @layer prep
// @role prep/alg8：η=2 centered binomial（secret/noise）设备采样与 LUT；与 presample/alg7 链接成 prep 链。 / Alg.8 η=2 CBD prep helpers. 本文件 `cbd2_ab_lut.h` 为该子模块组件。 / Component: cbd2_ab_lut.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。

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
