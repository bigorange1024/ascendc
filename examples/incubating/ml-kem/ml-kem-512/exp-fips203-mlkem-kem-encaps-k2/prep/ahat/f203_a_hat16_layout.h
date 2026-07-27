// @probe exp-fips203-mlkem-kem-encaps-k2
// @file prep/ahat/f203_a_hat16_layout.h
// @layer prep
// @role prep/ahat：设备侧生成 Encrypt 用矩阵 A_hat（FIPS203 Alg.14 行 3–7）；AIV-only UB 流水，为 compute MMAD 提供 a_hat GM。
// @production_io 默认 run.sh 生产 I/O：input/ek_pke.bin(800B)+m.bin+coins.bin；output/c.bin(768B)；a_hat/re 仅为 device arena 中间态。
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_a_hat16_config.h, f203_alg7_layout.h, cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 1 Â[4,256] 分片构建。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-512（k=2）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：prep/ahat/f203_a_hat16_layout.h
 */
/**
 * @file f203_a_hat16_layout.h
 * @brief Alg.13 行 3–7：4× SampleNTT 几何与 GM 布局（k=2，与单 poly alg7 几何锁定一致）。
 *
 * 流水线：ML-KEM-512 D14 prep 复用 k2 B4 Â 组件；XOF/d12/rej 常量与
 * pass-fix-f203-alg7-sample-ntt-k2 的 f203_alg7_layout.h 同值。
 *
 * GM 布局与参数卡 §3.2 一致：Â[4,256]，无 16-poly padding。
 */
#pragma once

#include "f203_a_hat16_config.h"
#include "f203_alg7_layout.h"

#include <cstdint>

namespace F203Ahat16 {

constexpr uint32_t kKyberK = 2U;
constexpr uint32_t kKyberN = 256U;
constexpr uint32_t kKyberQ = 3329U;

constexpr uint32_t kRhoBytes = 32U;
constexpr uint32_t kSampleSeedBytes = 34U;

constexpr uint32_t kShake128Rate = 168U;
constexpr uint32_t kXofSqueezeBlocks = F203Alg7::kXofSqueezeBlocks;
constexpr uint32_t kXofBytes = F203Alg7::kXofBytes;
constexpr uint32_t kCandPairs = F203Alg7::kCandPairs;
constexpr uint32_t kStreamLen = F203Alg7::kStreamLen;

constexpr uint32_t kAHatPolys = kKyberK * kKyberK;  // 4
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
