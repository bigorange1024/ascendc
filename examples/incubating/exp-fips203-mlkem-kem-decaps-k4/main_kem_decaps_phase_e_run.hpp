/**
 * @file main_kem_decaps_phase_e_run.hpp
 * @brief Phase-E Host：G(m'‖h)+Encrypt+FO → K。
 */
#pragma once

#include <cstdint>

/**
 * Alg.18 行 6–12。
 * @param ek/m_prime/h/z/c  Host 输入（各 1568/32/32/32/1568）
 * @param K_out             32B 最终 K
 * @return 0 成功
 *
 * SIM：自含 aclInit…aclFinalize（供 decaps_2session 第二段）。
 * 读 ./input/lut_ntt_* 与 lut_intt_*；CPU 另读 golden_v.bin。
 */
int RunKemDecapsPhaseE(const uint8_t *ek, const uint8_t *m_prime, const uint8_t *h, const uint8_t *z,
                       const uint8_t *c, uint8_t *K_out);
