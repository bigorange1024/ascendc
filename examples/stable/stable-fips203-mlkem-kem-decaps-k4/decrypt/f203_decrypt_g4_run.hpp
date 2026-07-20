/**
 * @file f203_decrypt_g4_run.hpp
 * @brief Decrypt Host 编排声明：单 kernel 全链，仅写出 m。
 *
 * Decrypt 流水线（1-kernel fused）Host 侧入口声明；实现见 main_decrypt_g4_run.cpp。
 * 对齐 FIPS 203 Alg.15：输入 dk_PKE + c + 静态 LUT → 输出 m[32]。
 * golden I/O：调用方读 input/，本函数写 output/m.bin；中间态不 D2H、不落盘。
 */
#pragma once

#include <cstdint>
#include <string>

/**
 * 生产入口：H2D(dk,c,LUT) → f203_decrypt_device_fused → D2H(m) → 写 output/m.bin。
 * @param case_dir 用例根目录（含 input/、output/）
 * @param dk/c     Host 缓冲：1536B / 1568B
 * @param lut_*    NTT/INTT even/odd stacked LUT（各 tiling::lutEvenOddFileBytes）
 * @param m_out    输出 m[32] Host 缓冲（同时落盘）
 * @return 0 成功，非 0 失败
 */
int run_decrypt_device_full(const std::string &case_dir, const uint8_t *dk, const uint8_t *c,
                            const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                            const uint8_t *lut_intt_odd, uint8_t *m_out);
