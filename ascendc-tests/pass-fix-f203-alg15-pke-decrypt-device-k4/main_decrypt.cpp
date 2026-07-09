/**
 * @file main_decrypt.cpp
 * @brief Alg.15 Decrypt 预研 Host 入口：读生产输入 → 调设备全链 → 写出 m。
 *
 * 流水线位置：Host 壳；密码学全在 device（f203_decrypt_device_fused）。
 * customspec：exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.tex
 *
 * 生产 I/O（严格对齐 FIPS 203 Alg.15）：
 *   读 input/dk_pke.bin (1536B)、input/c.bin (1568B)、input/lut_*_stacked.bin×4
 *   写 output/m.bin (32B)（由 run_decrypt_device_full 完成）
 * 禁止依赖 input/ 中的 ek_pke / m / coins / meta（那些只是 gen_data 造 c 的夹具）。
 */
#include "data_utils.h"
#include "f203_decrypt_g4_run.hpp"
#include "f203_decrypt_layout.h"
#include "f203_decrypt_ntt_u_tiling.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    const std::string case_dir = ".";

    /* ---- 生产输入：dk + c ---- */
    std::vector<uint8_t> dk_buf(F203_DK_PKE_BYTES);
    std::vector<uint8_t> c_buf(F203_CT_PKE_BYTES);
    size_t rs = 0;
    if (!ReadFile(case_dir + "/input/dk_pke.bin", rs, dk_buf.data(), dk_buf.size()) || rs != F203_DK_PKE_BYTES) {
        std::fprintf(stderr, "[main_decrypt] bad dk_pke.bin size=%zu\n", rs);
        return 1;
    }
    if (!ReadFile(case_dir + "/input/c.bin", rs, c_buf.data(), c_buf.size()) || rs != F203_CT_PKE_BYTES) {
        std::fprintf(stderr, "[main_decrypt] bad c.bin size=%zu\n", rs);
        return 2;
    }

    /* ---- 静态 NTT/INTT LUT（与 SEED 无关；装入设备 workspace）---- */
    std::vector<uint8_t> lut_even(tiling::lutEvenOddFileBytes);
    std::vector<uint8_t> lut_odd(tiling::lutEvenOddFileBytes);
    std::vector<uint8_t> lut_intt_even(tiling::lutEvenOddFileBytes);
    std::vector<uint8_t> lut_intt_odd(tiling::lutEvenOddFileBytes);
    if (!ReadFile(case_dir + "/input/lut_even_stacked.bin", rs, lut_even.data(), lut_even.size()) ||
        rs != lut_even.size()) {
        return 3;
    }
    if (!ReadFile(case_dir + "/input/lut_odd_stacked.bin", rs, lut_odd.data(), lut_odd.size()) ||
        rs != lut_odd.size()) {
        return 4;
    }
    if (!ReadFile(case_dir + "/input/lut_intt_even_stacked.bin", rs, lut_intt_even.data(), lut_intt_even.size()) ||
        rs != lut_intt_even.size()) {
        return 5;
    }
    if (!ReadFile(case_dir + "/input/lut_intt_odd_stacked.bin", rs, lut_intt_odd.data(), lut_intt_odd.size()) ||
        rs != lut_intt_odd.size()) {
        return 6;
    }

    /* ---- 设备全链；仅 D2H m ---- */
    std::vector<uint8_t> m_out(F203_MSG_BYTES);
    std::printf("[main_decrypt] Alg.15 production (dk+c+LUT → m only, no mid D2H)\n");
    return run_decrypt_device_full(case_dir, dk_buf.data(), c_buf.data(), lut_even.data(), lut_odd.data(),
                                   lut_intt_even.data(), lut_intt_odd.data(), m_out.data());
}
