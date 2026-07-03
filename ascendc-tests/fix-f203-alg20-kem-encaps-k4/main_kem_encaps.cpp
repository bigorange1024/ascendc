/**
 * @file main_kem_encaps.cpp
 * @brief Alg.20 ML-KEM.Encaps(ek) host 入口：读 ek_kem + seed，写 c/K。
 *
 * 生产：input/seed_d.bin（4B）→ device DerandMFromSeedD。
 * 旁路 A（KEM_ENC_EXT_SEED=1，test-only）：input/encaps_seed.bin（32B m）直接喂 device。
 */
#include "data_utils.h"
#include "f203_encrypt_layout.h"
#include "f203_kem_enc_layout.h"
#include "f203_ntt_r_tiling.h"
#include "main_kem_enc_g5_run.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    const std::string case_dir = ".";

    std::vector<uint8_t> ek_buf(F203_EK_PKE_BYTES);
    size_t rs = 0;
    if (!ReadFile(case_dir + "/input/ek_kem.bin", rs, ek_buf.data(), ek_buf.size()) || rs != F203_EK_PKE_BYTES) {
        std::fprintf(stderr, "[main_kem_enc] bad ek_kem.bin size=%zu\n", rs);
        return 1;
    }

    std::vector<uint8_t> seed_buf(F203KemEnc::kSeedGmBytes);
#if KEM_ENC_EXT_SEED
    if (!ReadFile(case_dir + "/input/encaps_seed.bin", rs, seed_buf.data(), seed_buf.size()) ||
        rs != seed_buf.size()) {
        std::fprintf(stderr, "[main_kem_enc] bad encaps_seed.bin (KEM_ENC_EXT_SEED)\n");
        return 2;
    }
#else
    uint32_t seed_d = 20260619U;
    if (!ReadFile(case_dir + "/input/seed_d.bin", rs, &seed_d, sizeof(seed_d)) || rs != sizeof(seed_d)) {
        std::fprintf(stderr, "[main_kem_enc] bad seed_d.bin\n");
        return 2;
    }
    std::memcpy(seed_buf.data(), &seed_d, sizeof(seed_d));
#endif

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

    std::vector<uint8_t> c_out(F203_CT_PKE_BYTES);
    std::vector<uint8_t> K_out(F203KemEnc::kSharedSecretBytes);

#ifdef ASCENDC_CPU_DEBUG
    const int rc = run_kem_enc_g5_cpu_full(case_dir, ek_buf.data(), seed_buf.data(), lut_even.data(), lut_odd.data(),
                                           lut_intt_even.data(), lut_intt_odd.data(), c_out.data(), K_out.data());
    return rc != 0 ? rc : 0;
#else
    const int rc = run_g5_sim_full(ek_buf.data(), seed_buf.data(), lut_even.data(), lut_odd.data(),
                                   lut_intt_even.data(), lut_intt_odd.data(), c_out.data(), K_out.data());
    if (rc != 0) {
        return rc;
    }
    if (!WriteFile(case_dir + "/output/c.bin", c_out.data(), c_out.size())) {
        return 10;
    }
    if (!WriteFile(case_dir + "/output/K.bin", K_out.data(), K_out.size())) {
        return 11;
    }
    std::printf("[main_kem_enc] SIM OK c=%uB K=%uB\n", F203_CT_PKE_BYTES, F203KemEnc::kSharedSecretBytes);
    return 0;
#endif
}
