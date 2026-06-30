/**
 * @file f203_encrypt_g5_run.hpp
 * @brief G5 生产路径：设备 ByteDecode ek→t̂；CPU/SIM 单 session 全链。
 */
#pragma once

#include <cstdint>
#include <string>

/** CPU：单 session prep..pack → c.bin。 */
int run_encrypt_g5_cpu_full(const std::string &case_dir, const uint8_t *ek, const uint8_t *coins, const uint8_t *m,
                              const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                              const uint8_t *lut_intt_odd, uint8_t *c_out);

/**
 * SIM 全链：单 ACL session prep..pack → c.bin（INTEGRATION_PLAN §4）。
 *
 * 含 prep / NTT / decode(MIX) / at_r5 / INTT×2 / g4_noise / pack(MIX)；
 * D2H 中间张量供 verify_gate；c_out 为 device pack 输出。
 */
int run_g5_sim_full(const uint8_t *ek, const uint8_t *coins, const uint8_t *m, uint8_t *a_hat, uint8_t *re_flat,
                    uint8_t *r_hat, uint8_t *t_hat, uint8_t *u_hat, uint8_t *tr_hat, const uint8_t *lut_ntt_even,
                    const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                    uint8_t *c_out);
