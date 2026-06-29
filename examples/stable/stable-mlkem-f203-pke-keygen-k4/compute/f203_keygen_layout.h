// @probe stable-mlkem-f203-pke-keygen-k4
// @file compute/f203_keygen_layout.h
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `f203_keygen_layout.h` 为该子模块组件。 / Component: f203_keygen_layout.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file f203_keygen_layout.h
 * @brief Alg.13 KeyGen 行 21 I/O 尺寸（k=4，ML-KEM-768）。
 */
#pragma once

#include <cstdint>

namespace F203Keygen {

constexpr uint32_t kKyberK = 4U;
constexpr uint32_t kPolyBytesEncoded = 384U;
constexpr uint32_t kEkPolyvecBytes = kKyberK * kPolyBytesEncoded;  // 1536
constexpr uint32_t kRhoBytes = 32U;
constexpr uint32_t kEkPkeBytes = kEkPolyvecBytes + kRhoBytes;      // 1568
constexpr uint32_t kDkPkeBytes = kEkPolyvecBytes;                  // 1536

}  // namespace F203Keygen
