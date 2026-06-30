#pragma once

#include <cstdint>
#include <string>

int run_decrypt_g4_cpu_full(const std::string &case_dir, const uint8_t *dk, const uint8_t *c,
                              const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd,
                              const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd, uint8_t *m_out);

int run_decrypt_g4_sim_full(const uint8_t *dk, const uint8_t *c, uint8_t *u, uint8_t *v, uint8_t *s_hat,
                              uint8_t *u_hat, uint8_t *w_hat, uint8_t *w_time, const uint8_t *lut_ntt_even,
                              const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                              const uint8_t *lut_intt_odd, uint8_t *m_out);
