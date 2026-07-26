// @probe pass-fix-f203-alg14-pke-encrypt-device-k3
// @file prep/presample/f203_se_vector_g.hpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样
// @production_io Encrypt prep：input ek_pke.bin+coins.bin；output a_hat.bin+re.bin；中间 prf 不落盘。
// @launch prep launch: blockDim=2, AIV_ONLY
// @ai_core SIM：0×AIC+2×AIV；双 AIV 并行 Â；block0 独占 PRF+CBD。
// @depends 见文件内 #include
// @verify run.sh CPU+SIM；verify_result.py max_abs_diff=0。


/**
 * @file f203_se_vector_g.hpp
 * @brief Phase G：SEED_D → σ（与 Alg.7 / KeyGen 同式，一次 G 派生 ρ‖σ）。
 *
 * 流水线：KeyGen/presample 在 PRF 前取 σ；Encrypt prep 不经 G（coins 直接作 PRF 密钥）。
 * 内部与 Â 路径共用 F203Alg7::BuildRhoSigmaFromSeedD（一次 Derand + SHA3-512）。
 */
#pragma once

#include "f203_alg7_g.hpp"

#include <cstdint>

namespace F203SeVector {

constexpr uint32_t K = 4U;

/**
 * SEED_D → σ[32]；ρ 写入后丢弃（本 Phase 只要 σ）。
 * @param seed_d derand 后的 32-bit 种子；@param sigma 输出 32B
 */
__aicore__ inline void BuildSigmaFromSeedD(uint32_t seed_d, uint8_t sigma[32])
{
    uint8_t rho_unused[32];
    F203Alg7::BuildRhoSigmaFromSeedD(seed_d, rho_unused, sigma);
}

}  // namespace F203SeVector
