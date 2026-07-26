// @probe pass-fix-f203-alg13-device-keygen-k3
// @file prep/ahat/f203_a_hat16_layout.h
// @layer prep
// @role prep/ahat：设备侧生成矩阵 A_hat（FIPS203 Alg.6/布局 f203_a_hat16）；AIV-only UB 流水，为 compute MMAD 提供 a_hat GM。 / Device A_hat generation for keygen prep. 本文件 `f203_a_hat16_layout.h` 为该子模块组件。 / Component: f203_a_hat16_layout.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1184B) + dk_pke.bin (1152B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_a_hat16_config.h, f203_alg7_layout.h, cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 1 Â[9,256] 分片构建。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-768（k=3）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：prep/ahat/f203_a_hat16_layout.h
 */
/**
 * @file f203_a_hat16_layout.h
 * @brief Alg.13 行 3–7：9× SampleNTT 几何与 GM 布局（k=3，与单 poly alg7 几何锁定一致）。
 *
 * 流水线：pass-fix-f203-alg13-lines3-7-a-hat-k4 专用；XOF/d12/rej 常量与
 * pass-fix-f203-alg7-sample-ntt-k4 的 f203_alg7_layout.h 同值。
 *
 * GM 布局与 vec-k4 hat_dot_layout::a_hat_offset(p,j) 一致。
 */
#pragma once

#include "f203_a_hat16_config.h"
#include "f203_alg7_layout.h"

#include <cstdint>

namespace F203Ahat16 {

constexpr uint32_t kKyberK = 3U;
constexpr uint32_t kKyberN = 256U;
constexpr uint32_t kKyberQ = 3329U;

constexpr uint32_t kRhoBytes = 32U;
constexpr uint32_t kSampleSeedBytes = 34U;

constexpr uint32_t kShake128Rate = 168U;
constexpr uint32_t kXofSqueezeBlocks = F203Alg7::kXofSqueezeBlocks;
constexpr uint32_t kXofBytes = F203Alg7::kXofBytes;
constexpr uint32_t kCandPairs = F203Alg7::kCandPairs;
constexpr uint32_t kStreamLen = F203Alg7::kStreamLen;

constexpr uint32_t kAHatPolys = kKyberK * kKyberK;  // 9
constexpr uint32_t kAHatBytes = kAHatPolys * kKyberN * sizeof(int32_t);
constexpr uint32_t kShakeBatch = kAHatPolys;
constexpr uint32_t kXofBatchBytes = kShakeBatch * kXofBytes;

constexpr uint32_t kD12Bytes = kCandPairs * sizeof(int32_t);
constexpr uint32_t kPolyAHatBytes = kKyberN * sizeof(int32_t);

/** 行主序：Â[p,j] 在扁平 a_hat GM 中的起始 int32 下标（Host / golden 用）。 */
constexpr uint32_t AHatOffset(uint32_t p, uint32_t j)
{
    return (p * kKyberK + j) * kKyberN;
}

/** polyIdx = p*K+j 时的 (p,j)。遍历顺序：p 外、j 内。 */
constexpr void PolyIdxToPJ(uint32_t polyIdx, uint8_t &p, uint8_t &j)
{
    p = static_cast<uint8_t>(polyIdx / kKyberK);
    j = static_cast<uint8_t>(polyIdx % kKyberK);
}

}  // namespace F203Ahat16
