/**
 * @file main_kem_decaps.cpp
 * @brief FIPS 203 Alg.21 ML-KEM.Decaps(dk, c) host 入口。
 *
 * 读 input/dk_kem.bin（Alg.19）+ c.bin（Alg.20）+ LUT，调用
 * run_kem_decaps_cpu_full / run_kem_decaps_sim_full，写 output/K.bin。
 * 密码学全在 device（Decrypt G4 + G + Re-Encrypt G5 + 设备 FO）；本文件只做 I/O。
 */
#include "data_utils.h"
#include "f203_kem_dec_layout.h"
#include "f203_ntt_r_tiling.h"
#include "main_kem_dec_g5_run.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    const std::string case_dir = ".";

    std::vector<uint8_t> dk_buf(F203KemDec::kDkKemBytes);
    std::vector<uint8_t> c_buf(F203KemDec::kCtBytes);
    size_t rs = 0;
    if (!ReadFile(case_dir + "/input/dk_kem.bin", rs, dk_buf.data(), dk_buf.size()) ||
        rs != F203KemDec::kDkKemBytes) {
        std::fprintf(stderr, "[main_kem_decaps] bad dk_kem.bin size=%zu\n", rs);
        return 1;
    }
    if (!ReadFile(case_dir + "/input/c.bin", rs, c_buf.data(), c_buf.size()) || rs != F203KemDec::kCtBytes) {
        std::fprintf(stderr, "[main_kem_decaps] bad c.bin size=%zu\n", rs);
        return 2;
    }

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

    std::vector<uint8_t> K_out(F203KemDec::kSharedSecretBytes);

#ifdef ASCENDC_CPU_DEBUG
    const int rc = run_kem_decaps_cpu_full(case_dir, dk_buf.data(), c_buf.data(), lut_even.data(), lut_odd.data(),
                                           lut_intt_even.data(), lut_intt_odd.data(), K_out.data());
    return rc != 0 ? rc : 0;
#else
    const int rc = run_kem_decaps_sim_full(dk_buf.data(), c_buf.data(), lut_even.data(), lut_odd.data(),
                                           lut_intt_even.data(), lut_intt_odd.data(), K_out.data());
    if (rc != 0) {
        return rc;
    }
    if (!WriteFile(case_dir + "/output/K.bin", K_out.data(), K_out.size())) {
        return 10;
    }
    std::printf("[main_kem_decaps] SIM OK K=%uB\n", F203KemDec::kSharedSecretBytes);
    return 0;
#endif
}
