#pragma once

#include <cstdint>
#include <string>

/** 生产入口：仅写出 output/m.bin；中间态不落盘、不 D2H。 */
int run_decrypt_device_full(const std::string &case_dir, const uint8_t *dk, const uint8_t *c,
                            const uint8_t *lut_ntt_even, const uint8_t *lut_ntt_odd, const uint8_t *lut_intt_even,
                            const uint8_t *lut_intt_odd, uint8_t *m_out);
