/**
 * @file main_kem_dec_g5_run.hpp
 * @brief Alg.21 KEM Decaps host 编排声明。
 *
 * CPU：单 session Decrypt G4 + G + Encrypt G5 + 设备 FO。
 * SIM：默认 2-session（Phase-D 后 aclFinalize，fresh session Phase-E+FO），规避 CAModel 单 session c' 污染。
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
