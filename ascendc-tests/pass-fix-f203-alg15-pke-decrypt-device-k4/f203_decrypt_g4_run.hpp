/**
 * @file f203_decrypt_g4_run.hpp
 * @brief Host 侧 Alg.15 Decrypt 生产入口声明。
 *
 * 流水线位置：main_decrypt.cpp → run_decrypt_device_full（实现见 main_decrypt_g4_run.cpp）。
 * 契约：H2D dk+c+LUT → 单 kernel f203_decrypt_device_fused → 仅 D2H m；
 * 中间态不落盘、不对 Host 暴露。
 */
#pragma once

#include <cstdint>
#include <string>

/**
 * 生产入口：设备全链 Decrypt，仅写出 output/m.bin（及返回缓冲 m_out）。
 *
 * @param case_dir 用例根（含 input/ output/）
 * @param dk / c   Host 侧 dk_PKE(1536)、c(1568)
 * @param lut_*    NTT/INTT even/odd stacked LUT（与 gen_data 一致）
 * @param m_out    至少 32B；成功时写入明文
 * @return 0 成功，非 0 失败（读文件 / 对拍前错误）
 */
int run_decrypt_device_full(const std::string &case_dir, const uint8_t *dk, const uint8_t *c,
                            const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                            const uint8_t *lut_intt_odd, uint8_t *m_out);
