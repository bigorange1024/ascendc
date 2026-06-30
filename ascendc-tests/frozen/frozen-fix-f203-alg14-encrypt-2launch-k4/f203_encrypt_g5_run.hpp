/**
 * @file f203_encrypt_g5_run.hpp
 * @brief G5 生产路径：设备 ByteDecode ek→t̂；CPU/SIM 均单 session 全链至 c.bin。
 */
#pragma once

#include <cstdint>
#include <string>

/** CPU：单 session prep..pack → c.bin。 */
int run_encrypt_g5_cpu_full(const std::string &case_dir, const uint8_t *ek, const uint8_t *coins, const uint8_t *m,
                              const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                              const uint8_t *lut_intt_odd, uint8_t *c_out);

#ifndef ASCENDC_CPU_DEBUG
/** SIM/NPU：单 session prep..pack → c.bin（编排对齐 run_g5_cpu_session，禁止多段 aclFinalize）。 */
int run_encrypt_g5_sim_full(const std::string &case_dir, const uint8_t *ek, const uint8_t *coins, const uint8_t *m,
                            const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                            const uint8_t *lut_intt_odd, uint8_t *c_out);

/**
 * tail-only 快跑：读 output/golden_{u_hat,tr_hat,e1,e2}.bin + input/m.bin，仅跑 INTT→noise→pack。
 * 始终 dump sim_{u_time,tr_time,u_noisy,v}.bin，供 scripts/verify_g4_tail.py 分阶段对拍。
 * 前置：先运行 scripts/host_golden/gen_g4_tail_golden.py 生成 golden 中间量。
 */
int run_encrypt_g4_tail_only_sim(const std::string &case_dir, const uint8_t *lut_intt_even,
                                 const uint8_t *lut_intt_odd, uint8_t *c_out);
#endif
