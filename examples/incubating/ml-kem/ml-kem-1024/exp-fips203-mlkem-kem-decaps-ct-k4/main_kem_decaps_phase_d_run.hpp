/**
 * @file main_kem_decaps_phase_d_run.hpp
 * @brief Phase-D Host API：stable Decrypt fused；dk_kem 前缀即 dk_pke。
 *
 * 见 `main_kem_decaps_phase_d_run.cpp`。SIM 本段自含 aclInit…aclFinalize（decaps_2session 第一段）。
 */
#pragma once

#include <cstdint>

/**
 * Alg.18 行 1–5：m' ← K-PKE.Decrypt(dk_pke, c)。
 *
 * @param dk_kem        3168B；设备只读前缀 [0:1536) 作 dk_pke
 * @param c             1568B 密文
 * @param lut_even/odd  Decrypt 正向 NTT stacked LUT
 * @param lut_intt_*    Decrypt INTT stacked LUT
 * @param m_out         输出 32B m'（Host 缓冲）
 * @return 0 成功；非 0 为 ACL/读写失败码
 */
int RunKemDecapsPhaseD(const uint8_t *dk_kem, const uint8_t *c, const uint8_t *lut_even,
                       const uint8_t *lut_odd, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                       uint8_t *m_out);
