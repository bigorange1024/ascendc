/**
 * @file f203_encrypt_g5_run.hpp
 * @brief G5 生产路径：设备 ByteDecode ek→t̂；CPU 单 session 全链；SIM phase1 单 session 至 G3。
 */
#pragma once

#include <cstdint>
#include <string>

/** CPU：单 session prep..pack → c.bin。 */
int run_encrypt_g5_cpu_full(const std::string &case_dir, const uint8_t *ek, const uint8_t *coins, const uint8_t *m,
                              const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                              const uint8_t *lut_intt_odd, uint8_t *c_out);

/** SIM phase1：单 session prep..decode + at_r→u_hat + D2H aCol0；tr_hat 由 main 调 G4 独立 session at_r。 */
int run_g5_sim_phase1(const uint8_t *ek, const uint8_t *coins, uint8_t *a_hat, uint8_t *re_flat, uint8_t *r_hat,
                      uint8_t *t_hat, uint8_t *u_hat, uint8_t *a_col0_out, const uint8_t *lut_ntt_even,
                      const uint8_t *lut_ntt_odd);
