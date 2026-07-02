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

/** SIM 全链：单 ACL session prep..pack → c_out（仅算法输出落盘由 caller 负责）。 */
int run_g5_sim_full(const uint8_t *ek, const uint8_t *coins, const uint8_t *m, const uint8_t *lut_ntt_even,
                    const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                    uint8_t *c_out);
