// @probe exp-fips203-mlkem-pke-keygen-k4
// @file f203_keygen_layout.h
// @layer host
// @role 头文件/内联：`f203_keygen_layout.h` 声明或配置 AscendC/host 接口与常量。 / Header `f203_keygen_layout.h`.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch N/A（host / 脚本 / CMake 不参与 device launch）
// @ai_core N/A（非 AI Core 内核源）
// @depends #include: cstdint
// @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。


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
