/**
 * @file main_kem_decaps_phase_e_run.hpp
 * @brief Phase-E Host API：G(m'‖h) + Encrypt + 设备 FO → 最终 K。
 *
 * SIM：本段自含 aclInit…aclFinalize（decaps_2session 对照路径第二段）。
 * CPU：读 `./input/golden_v.bin` 作 v（与 encaps 的 m 一致时由 gen_data 生成；缺 M_FILE 会假拒绝）。
 */
#pragma once

#include <cstdint>

/**
 * Alg.18 行 6–12。
 * @param ek       800B 公钥（自 dk 切片或 Phase-E-only 文件）
 * @param m_prime  32B Decrypt 明文
 * @param h        32B H(ek)
 * @param z        32B 拒绝种子
 * @param c        768B 输入密文（FO 比较侧）
 * @param K_out    32B 最终共享秘密
 * @return 0 成功
 */
int RunKemDecapsPhaseE(const uint8_t *ek, const uint8_t *m_prime, const uint8_t *h, const uint8_t *z,
                       const uint8_t *c, uint8_t *K_out);
