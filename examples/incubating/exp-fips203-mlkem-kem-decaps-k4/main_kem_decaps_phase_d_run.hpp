/**
 * @file main_kem_decaps_phase_d_run.hpp
 * @brief Phase-D Host：stable Decrypt fused；dk_kem 前缀即 dk_pke（不另开切片 launch）。
 */
#pragma once

#include <cstdint>

/**
 * Alg.18 行 1–5：m' ← Decrypt(dk_pke, c)。
 * @param dk_kem 3168B（设备只读 [0:1536)）
 * @param c      1568B
 * @param lut_*  Decrypt NTT/INTT stacked LUT（各 tiling::lutEvenOddFileBytes）
 * @param m_out  32B m'
 * @return 0 成功
 */
int RunKemDecapsPhaseD(const uint8_t *dk_kem, const uint8_t *c, const uint8_t *lut_even,
                       const uint8_t *lut_odd, const uint8_t *lut_intt_even, const uint8_t *lut_intt_odd,
                       uint8_t *m_out);
