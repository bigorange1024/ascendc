/**
 * @file main_kem_enc_g5_run.hpp
 * @brief Alg.20 KEM Encaps：vendor Encrypt G5 + device KEM 头。
 */
#pragma once

#include <cstdint>
#include <string>

int run_kem_enc_g5_cpu_full(const std::string &case_dir, const uint8_t *ek, const uint8_t *seed_host,
                            const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                            const uint8_t *lut_intt_odd, uint8_t *c_out, uint8_t *K_out);

/** SIM 全链：仅 D2H 算法输出 c、K（中间态留在 device GM，不落盘）。 */
int run_g5_sim_full(const uint8_t *ek, const uint8_t *seed_host, const uint8_t *lut_ntt_even,
                    const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                    uint8_t *c_out, uint8_t *K_out);
