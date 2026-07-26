// @probe pass-fix-f203-alg19-kem-keygen-device-k3
// @file f203_keygen_prep_layout.h
// @layer host
// @role 头文件/内联：`f203_keygen_prep_layout.h` 声明或配置 AscendC/host 接口与常量。 / Header `f203_keygen_prep_layout.h`.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_kem.bin (1184B) + dk_kem.bin (2400B)；D13 PKE 中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_kem+dk_kem out; no intermediate GM dumps.
// @launch N/A（host / 脚本 / CMake 不参与 device launch）
// @ai_core N/A（非 AI Core 内核源）
// @depends #include: cstdint
// @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。


/**
 * @file f203_keygen_prep_layout.h
 * @brief KeyGen 准备段（Launch 1）GM / tiling 尺寸常量。
 *
 * ## 流水线位置
 * 行 3–15：Â[9×256] + ŝ/ê[6×256] + ρ[32] + PRF 中间态；
 * Host `main_keygen` 与设备 `BuildKeygenPrepSinglePipe` 共用本命名空间尺寸。
 *
 * ## 对齐
 * FIPS 203 Alg.13，ML-KEM-768（k=3，N=256）；与 golden 中间态形状一致（生产默认不落盘）。
 *
 * ## 分核
 * F203_AHAT16_BLOCK_DIM=2：双 AIV 按 5+4 写 9 个 Â poly；PRF/CBD 仅 block0。
 */
#pragma once

#include <cstdint>

#ifndef F203_AHAT16_BLOCK_DIM
#define F203_AHAT16_BLOCK_DIM 2
#endif

namespace F203KeygenPrep {

constexpr uint32_t kKyberK = 3U;
constexpr uint32_t kKyberN = 256U;
/** Â 多项式个数：k×k = 9 */
constexpr uint32_t kAHatPolys = kKyberK * kKyberK;
/** src 行数：ŝ 的 k 行 + ê 的 k 行 = 6 */
constexpr uint32_t kSrcRows = 2U * kKyberK;

/** presample：6 路并行 PRF（对应 3s+3e） */
constexpr uint32_t kSeBatch = kSrcRows;
constexpr uint32_t kSeMaxMsgLen = 64U;
/** CBD_η=2 每 poly 需 128B 随机（η·N/4） */
constexpr uint32_t kSePrfOutLen = 128U;

constexpr size_t kSeedBytes = sizeof(uint32_t);
constexpr size_t kRhoBytes = 32U;
/** Â GM：9×256×sizeof(int32) */
constexpr size_t kAHatBytes = static_cast<size_t>(kAHatPolys) * kKyberN * sizeof(int32_t);
/** src GM：6×256×sizeof(int32) */
constexpr size_t kSrcBytes = static_cast<size_t>(kSrcRows) * kKyberN * sizeof(int32_t);
constexpr size_t kPrfBytes = static_cast<size_t>(kSeBatch) * kSePrfOutLen;
constexpr size_t kSeXBytes = static_cast<size_t>(kSeBatch) * kSeMaxMsgLen;
constexpr size_t kSeLenBytes = static_cast<size_t>(kSeBatch) * sizeof(uint32_t);
constexpr size_t kSeWsBytes = 64U;

/** 与 `F203_AHAT16_BLOCK_DIM` 一致：1=单 AIV 9 poly；2=双 AIV 5+4（D13 锁定）。 */
constexpr uint32_t kPrepBlockDim = static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM);

}  // namespace F203KeygenPrep
