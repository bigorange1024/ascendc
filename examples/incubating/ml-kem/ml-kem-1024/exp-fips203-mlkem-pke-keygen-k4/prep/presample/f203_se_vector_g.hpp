
/** Phase G：从 seed 派生 ρ/σ 的设备辅助（与 Alg.7 G 一致）。 */
// @probe exp-fips203-mlkem-pke-keygen-k4
// @file prep/presample/f203_se_vector_g.hpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样与 NTT17 链入口；从 seed 派生设备侧中间量供 alg7/alg8/ahat。 / Presample + Keccak/PRF device vector entry. 本文件 `f203_se_vector_g.hpp` 为该子模块组件。 / Component: f203_se_vector_g.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_alg7_g.hpp, cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 1 行 8–15 PRF+CBD presample 链。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：prep/presample/f203_se_vector_g.hpp
 */
/**
 * @file f203_se_vector_g.hpp
 * @brief Phase G：SEED_D → σ（与 Alg.7 / KeyGen 同式，一次 G 派生 ρ‖σ）。
 */
#pragma once

#include "f203_alg7_g.hpp"

#include <cstdint>

namespace F203SeVector {

constexpr uint32_t K = 4U;

/** SEED_D → σ；内部与 Â 路径共用 F203Alg7::BuildRhoSigmaFromSeedD（一次 Derand + SHA3-512）。 */
__aicore__ inline void BuildSigmaFromSeedD(uint32_t seed_d, uint8_t sigma[32])
{
    uint8_t rho_unused[32];
    F203Alg7::BuildRhoSigmaFromSeedD(seed_d, rho_unused, sigma);
}

}  // namespace F203SeVector
