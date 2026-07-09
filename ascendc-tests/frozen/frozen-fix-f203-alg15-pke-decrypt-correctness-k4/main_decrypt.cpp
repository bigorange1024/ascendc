/**
 * @file main_decrypt.cpp
 * @brief Alg.15 Decrypt 探针 Host 入口（G4 生产路径）。
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

    std::vector<uint8_t> m_out(F203_MSG_BYTES);

#ifdef ASCENDC_CPU_DEBUG
    std::printf("[main_decrypt] G4 production single-session (device decrypt)\n");
    const int rc = run_decrypt_g4_cpu_full(case_dir, dk_buf.data(), c_buf.data(), lut_even.data(), lut_odd.data(),
                                         lut_intt_even.data(), lut_intt_odd.data(), m_out.data());
    return rc != 0 ? rc : 0;
#else
    std::vector<uint8_t> u(F203_U_POLYVEC_BYTES);
    std::vector<uint8_t> v(F203_V_POLY_BYTES);
    std::vector<uint8_t> s_hat(F203_S_HAT_BYTES);
    std::vector<uint8_t> u_hat(F203_U_HAT_BYTES);
    std::vector<uint8_t> w_hat(F203_W_HAT_BYTES);
    std::vector<uint8_t> w_time(F203_V_POLY_BYTES);
    std::printf("[main_decrypt] G4 SIM single-session\n");
    const int rc = run_decrypt_g4_sim_full(dk_buf.data(), c_buf.data(), u.data(), v.data(), s_hat.data(),
                                         u_hat.data(), w_hat.data(), w_time.data(), lut_even.data(), lut_odd.data(),
                                         lut_intt_even.data(), lut_intt_odd.data(), m_out.data());
    if (rc != 0) {
        return rc;
    }
    const std::string out_dir = case_dir + "/output";
    if (!WriteFile(out_dir + "/u.bin", u.data(), u.size()) || !WriteFile(out_dir + "/v.bin", v.data(), v.size()) ||
        !WriteFile(out_dir + "/s_hat.bin", s_hat.data(), s_hat.size()) ||
        !WriteFile(out_dir + "/u_hat.bin", u_hat.data(), u_hat.size()) ||
        !WriteFile(out_dir + "/w_hat.bin", w_hat.data(), w_hat.size()) ||
        !WriteFile(out_dir + "/w_time.bin", w_time.data(), w_time.size()) ||
        !WriteFile(out_dir + "/m.bin", m_out.data(), m_out.size())) {
        return 10;
    }
    std::printf("[main_decrypt] G4 done m.bin=%uB\n", F203_MSG_BYTES);
    return 0;
#endif
}
