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
