// @probe stable-fips203-mlkem-pke-keygen-k4
// @file f203_keygen_prep_layout.h
// @layer host
// @role 头文件/内联：`f203_keygen_prep_layout.h` 声明或配置 AscendC/host 接口与常量。 / Header `f203_keygen_prep_layout.h`.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch N/A（host / 脚本 / CMake 不参与 device launch）
// @ai_core N/A（非 AI Core 内核源）
// @depends #include: cstdint
// @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。


/**
 * @file f203_keygen_prep_layout.h
 * @brief KeyGen 准备段单内核 I/O 尺寸（行 3–15：Â + presample V3）。
 */
#pragma once

#include <cstdint>

#ifndef F203_AHAT16_BLOCK_DIM
#define F203_AHAT16_BLOCK_DIM 2
#endif

namespace F203KeygenPrep {

constexpr uint32_t kKyberK = 4U;
constexpr uint32_t kKyberN = 256U;
constexpr uint32_t kAHatPolys = kKyberK * kKyberK;
constexpr uint32_t kSrcRows = 8U;

constexpr uint32_t kSeBatch = 8U;
constexpr uint32_t kSeMaxMsgLen = 64U;
constexpr uint32_t kSePrfOutLen = 128U;

constexpr size_t kSeedBytes = sizeof(uint32_t);
constexpr size_t kRhoBytes = 32U;
constexpr size_t kAHatBytes = static_cast<size_t>(kAHatPolys) * kKyberN * sizeof(int32_t);
constexpr size_t kSrcBytes = static_cast<size_t>(kSrcRows) * kKyberN * sizeof(int32_t);
constexpr size_t kPrfBytes = static_cast<size_t>(kSeBatch) * kSePrfOutLen;
constexpr size_t kSeXBytes = static_cast<size_t>(kSeBatch) * kSeMaxMsgLen;
constexpr size_t kSeLenBytes = static_cast<size_t>(kSeBatch) * sizeof(uint32_t);
constexpr size_t kSeWsBytes = 64U;

/** 与 `F203_AHAT16_BLOCK_DIM` 一致：1=单 AIV 16 poly；2=双 AIV 8+8（Opt-4 默认）。 */
constexpr uint32_t kPrepBlockDim = static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM);

}  // namespace F203KeygenPrep
