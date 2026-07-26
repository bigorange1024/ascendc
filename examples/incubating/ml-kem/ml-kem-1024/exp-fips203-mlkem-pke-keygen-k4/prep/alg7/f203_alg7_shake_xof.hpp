// @probe exp-fips203-mlkem-pke-keygen-k4
// @file prep/alg7/f203_alg7_shake_xof.hpp
// @layer prep
// @role prep/alg7：FIPS203 Algorithm 7 SampleNTT（CBD+rej 采样 ŝ/ê 等）；含 XOF、rej 标量/向量与 compact LUT ROM。 / Alg.7 SampleNTT rejection-sampling prep kernels. 本文件 `f203_alg7_shake_xof.hpp` 为该子模块组件。 / Component: f203_alg7_shake_xof.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_alg7_layout.h, shake_general.h, shake_general_tiling_data.h, shake_ub_helpers.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file f203_alg7_shake_xof.hpp
 * @brief FIPS 203 Alg.7 第 5 行：ρ||byte(j)||byte(i) → SHAKE128 固定 squeeze 672B（全 UB I/O）。
 *
 * 流水线位置：d12_vec 全链中 SHAKE 段；输入/输出均为 UB LocalTensor，不经 GM。
 *
 * 672B 策略（见 f203_alg7_layout.h）：
 *   Kyber 首批 3×168B(504B) + 预取 tail 1×168B，等价于 squeeze(3 blocks)+squeeze(1 block) 续流；
 *   固定 224 组 (C0,C1,C2) 候选，避免 lazy while 与二次 XOF 分支。
 *
 * 与 golden 关系：F203_ALG7_DUMP_XOF=1 时 xof_ub 整段落 GM，与 golden/xof.bin 逐字节对拍。
 */
#pragma once

#include "f203_alg7_layout.h"
#include "shake_general.h"
#include "shake_general_tiling_data.h"
#include "shake_ub_helpers.hpp"

namespace F203Alg7 {

/**
 * 填充 SHAKE General 内核 tiling（batch=1 单 poly，消息 34B，输出 kXofBytes=672B）。
 * 与 library/shared/shake_xof_kernel tiling_host 语义一致。
 */
__aicore__ inline void FillAlg7ShakeTiling(ShakeGeneralTilingData &tiling)
{
    ShakeXofUb::FillShakeTilingUb(tiling, 1U, kSampleSeedBytes, kXofBytes, SHAKE128_RATE_BYTES);
}

/**
 * 构造 SHAKE absorb 消息与长度表（UB）。
 *
 * @param rho        ρ[32] 来自 BuildRhoFromSeedD
 * @param poly_j     矩阵行下标 j（1B）
 * @param poly_i     矩阵列下标 i（1B）
 * @param x_ub       UB 消息缓冲，至少 34B 有效；字节 33 为 poly_i
 * @param lengths_ub UB uint32[1]，写入 kSampleSeedBytes=34
 *
 * 布局：x_ub[0..31]=ρ，x_ub[32]=j，x_ub[33]=i（与 FIPS SampleNTT 一致）。
 */
__aicore__ inline void FillSampleSeedUb(const uint8_t rho[kRhoBytes], uint8_t poly_j, uint8_t poly_i,
                                        AscendC::LocalTensor<uint8_t> &x_ub, AscendC::LocalTensor<uint32_t> &lengths_ub)
{
    ShakeXofUb::FillShakeRowUb(rho, kRhoBytes, poly_j, x_ub, 0U);
    x_ub.SetValue(33U, poly_i);
    lengths_ub.SetValue(0U, kSampleSeedBytes);
}

/**
 * 执行 SHAKE128：一次 squeeze kXofBytes(672) 字节到 xof_ub。
 *
 * @param x_ub         已填充的 absorb 消息 UB
 * @param lengths_ub   消息长度 UB
 * @param xof_ub       输出 XOF 字节 UB[kXofBytes]
 * @param staging32    Keccak 内部 staging，尺寸 ≥ SHAKE_XOF_STAGING_BYTES
 *
 * 前置条件：tiling 由 FillAlg7ShakeTiling 生成；调用后须 PipeBarrier 再读 xof_ub。
 */
__aicore__ inline void RunShake128SampleNttUb(AscendC::LocalTensor<uint8_t> &x_ub, AscendC::LocalTensor<uint32_t> &lengths_ub,
                                              AscendC::LocalTensor<uint8_t> &xof_ub,
                                              AscendC::LocalTensor<uint8_t> &staging32)
{
    ShakeGeneralTilingData tilingLocal{};
    FillAlg7ShakeTiling(tilingLocal);
    ShakeXofUb::RunKernelShakeGeneralUb(x_ub, lengths_ub, xof_ub, staging32, &tilingLocal);
}

}  // namespace F203Alg7
