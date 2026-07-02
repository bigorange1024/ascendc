/**
 * @file main_kem_dec_g5_run.hpp
 * @brief Alg.21 KEM Decaps：单 session Decrypt G4 + Encrypt G5 + FO。
 */
#pragma once

#include <cstdint>
#include <string>

int run_kem_decaps_cpu_full(const std::string &case_dir, const uint8_t *dk_kem, const uint8_t *c_in,
                            const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                            const uint8_t *lut_intt_odd, uint8_t *K_out);

int run_kem_decaps_sim_full(const uint8_t *dk_kem, const uint8_t *c_in, const uint8_t *lut_ntt_even,
                            const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                            uint8_t *K_out);
