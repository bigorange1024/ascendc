/**
 * @file main_kem_enc_g5_run.hpp
 * @brief Alg.20 KEM Encaps host 编排声明：device KEM 头 + vendor Encrypt G5。
 *
 * CPU：run_kem_enc_g5_cpu_full（tikicpu 单 session）。
 * SIM：run_g5_sim_full（ACL 单 session；仅 D2H 算法输出 c、K）。
 */
#pragma once

#include <cstdint>
#include <string>

/**
 * CPU 孪生全链 Encaps。
 * @param case_dir 用例根（写 output/c.bin、K.bin）
 * @param ek / seed_host / lut_* 输入缓冲
 * @param c_out / K_out 输出 1568B / 32B
 */
int run_kem_enc_g5_cpu_full(const std::string &case_dir, const uint8_t *ek, const uint8_t *seed_host,
                            const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                            const uint8_t *lut_intt_odd, uint8_t *c_out, uint8_t *K_out);

/**
 * SIM 全链：仅 D2H 算法输出 c、K（中间态留在 device GM，不落盘）。
 * @return 0 成功；非 0 为 launch/ACL 错误码段
 */
int run_g5_sim_full(const uint8_t *ek, const uint8_t *seed_host, const uint8_t *lut_ntt_even,
                    const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                    uint8_t *c_out, uint8_t *K_out);
