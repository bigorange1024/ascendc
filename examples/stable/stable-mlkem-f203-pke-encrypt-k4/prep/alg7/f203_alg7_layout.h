// @probe stable-mlkem-f203-pke-keygen-k4
// @file prep/alg7/f203_alg7_layout.h
// @layer prep
// @role prep/alg7：FIPS203 Algorithm 7 SampleNTT（CBD+rej 采样 ŝ/ê 等）；含 XOF、rej 标量/向量与 compact LUT ROM。 / Alg.7 SampleNTT rejection-sampling prep kernels. 本文件 `f203_alg7_layout.h` 为该子模块组件。 / Component: f203_alg7_layout.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file f203_alg7_layout.h
 * @brief FIPS 203 Alg.7 SampleNTT 几何常量与 UB 尺寸闭包（单 poly POC，N=256，q=3329，k=4）。
 *
 * 流水线位置：被全链各模块 include；不含编译开关（见 f203_alg7_config.h）。
 *
 * XOF 长度策略（本探针工程决策，非 FIPS 条文原文）：
 * - SHAKE128 rate = 168B；Kyber/mlkem 首批启发式 GEN_MATRIX_NBLOCKS=3 → 504B（336 组三元组候选）。
 * - 业界 tail：同一次 absorb 上不足 256 接受时，每次再 squeeze **1×168B**（非再 squeeze 504）。
 * - 本探针 **不做 lazy while tail**：固定一次 squeeze **672B = 504+168**（4×rate）。
 *   语义等价于 Kyber `squeeze(3 blocks)` 后紧接 `squeeze(1 block)` 的续流字节；
 *   统计上 448 候选期望 ~363 接受，覆盖单块 504 偶发 <256 的尾情况。
 * - 向量管线优先 **满 repeat / 固定块长**，避免 rej 中途早停与二次 XOF 分支。
 * - **性能**：较 504B 档 SIM 约 +13% tick（多 1×rate Keccak）；详见 INTEGRATION_PLAN §1.5。
 *
 * 与 golden 关系：gen_data.py、verify_result.py 与本文件常量须一致；改 kXofBytes 须同步 golden。
 */
#pragma once

#include <cstdint>

namespace F203Alg7 {

/** ML-KEM 参数集（本探针锁定 k=4）。 */
constexpr uint32_t kKyberK = 4U;
constexpr uint32_t kKyberN = 256U;
constexpr uint32_t kKyberQ = 3329U;

/** ρ 长度；SampleNTT absorb 消息 = ρ[32] || byte(j) || byte(i)。 */
constexpr uint32_t kRhoBytes = 32U;
constexpr uint32_t kSampleSeedBytes = 34U;  // ρ[32] || byte(j) || byte(i)

/** SHAKE128 速率与预 squeeze 块数。 */
constexpr uint32_t kShake128Rate = 168U;
/** Kyber 首批块数（启发式 3×rate=504B）。 */
constexpr uint32_t kGenMatrixNBlocks = 3U;
#ifndef F203_ALG7_XOF_504
#define F203_ALG7_XOF_504 0
#endif
#if F203_ALG7_XOF_504
/** 504B 实验：仅 3×rate，无 tail prefetch（见 16-poly 探针 F203_ALG7_XOF_504 对照）。 */
constexpr uint32_t kTailPrefetchBlocks = 0U;
#else
/** 固定预 squeeze 的额外 rate 块（=业界 tail 单次增量 168B）。 */
constexpr uint32_t kTailPrefetchBlocks = 1U;
#endif
constexpr uint32_t kXofSqueezeBlocks = kGenMatrixNBlocks + kTailPrefetchBlocks;  // 4
constexpr uint32_t kXofBytes = kShake128Rate * kXofSqueezeBlocks;                // 672，32B 对齐
constexpr uint32_t kXofUbBytes = kXofBytes;                                      // UB 与语义等长
/** 候选三元组数：每 3 字节 (C0,C1,C2) 一组 → 672/3=224。 */
constexpr uint32_t kCandPairs = kXofBytes / 3U;

/** d1/d2 平面输出：各 kCandPairs 个 int32。 */
constexpr uint32_t kD12Bytes = kCandPairs * sizeof(int32_t);       // 896
constexpr uint32_t kKyberNHat = kKyberN;
/** â NTT 域多项式输出字节数。 */
constexpr uint32_t kAHatBytes = kKyberNHat * sizeof(int32_t);      // 1024
/** rej 交错 stream 长度：d1、d2 各 224 → 448 lane。 */
constexpr uint32_t kStreamLen = kCandPairs * 2U;

/** rej 剔除：Compares 128-lane tile（int32 count×4 须 256B 对齐 cmpMask）。 */
constexpr uint32_t kRejectFilterTileInt32 = 128U;
constexpr uint32_t kRejectFilterCmpMaskUbBytes = kRejectFilterTileInt32;
constexpr uint32_t kRejFilterMaskUbInt32 =
    (kRejectFilterCmpMaskUbBytes + static_cast<uint32_t>(sizeof(int32_t)) - 1U) / static_cast<uint32_t>(sizeof(int32_t));

/** rej 向量 WS int32 元素数：t1+t2+stream+idxRom + cmpMask(128B) + filterTile(128 int32)。 */
constexpr uint32_t kRejVecWsCoreInt32 = kCandPairs * 2U + kStreamLen * 2U;
constexpr uint32_t kRejVecWsInt32 = kRejVecWsCoreInt32 + kRejFilterMaskUbInt32 + kRejectFilterTileInt32;

/** d12 UB：c0,c1,c2,t0,t1（标量解交织路径 5×224 int32）。 */
constexpr uint32_t kD12WsScalarInt32 = kCandPairs * 5U;
/** d12 UB + 解交织 Gather 索引×3（实验路径 8×224 int32）。 */
constexpr uint32_t kD12WsGatherInt32 = kCandPairs * 8U;

}  // namespace F203Alg7
