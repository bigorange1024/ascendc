/**
 * @file main_kem_decaps.cpp
 * @brief stable-fips203-mlkem-kem-decaps-k4：Alg.21 Decaps 全链 Host（dk_kem + c → 仅 K）。
 *
 * customspec：stable-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex
 * SIM 默认 ASCENDC_SIM_HOST_MODE=decaps_1session；对照 decaps_2session（非默认）。
 * 行 1–4：dk 切片为指针偏移，不另开 launch。
 */
#include "ascendc_build_mode.hpp"
#include "data_utils.h"
#include "f203_kem_dec_layout.h"
#include "main_kem_decaps_phase_d_run.hpp"
#include "main_kem_decaps_phase_e_run.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    std::vector<uint8_t> dk(F203KemDec::kDkKemBytes);
    std::vector<uint8_t> c(F203KemDec::kCtBytes);
    std::vector<uint8_t> m(F203KemDec::kMsgBytes);
    std::vector<uint8_t> K(F203KemDec::kSharedSecretBytes);

    // Decrypt LUT（无 ntt_ 前缀）与 Encrypt LUT（lut_ntt_*）分文件
    constexpr size_t kDecLutBytes = 65536;  // tiling::lutEvenOddFileBytes（Decrypt）
    std::vector<uint8_t> lutEven(kDecLutBytes);
    std::vector<uint8_t> lutOdd(kDecLutBytes);
    std::vector<uint8_t> lutInttEven(kDecLutBytes);
    std::vector<uint8_t> lutInttOdd(kDecLutBytes);

    size_t rs = 0;
    if (!ReadFile("./input/dk_kem.bin", rs, dk.data(), dk.size()) || rs != dk.size()) {
        std::fprintf(stderr, "[kem-decaps] missing input/dk_kem.bin\n");
        return 12;
    }
    if (!ReadFile("./input/c.bin", rs, c.data(), c.size()) || rs != c.size()) {
        return 11;
    }
    if (!ReadFile("./input/lut_even_stacked.bin", rs, lutEven.data(), lutEven.size()) || rs != lutEven.size()) {
        return 20;
    }
    if (!ReadFile("./input/lut_odd_stacked.bin", rs, lutOdd.data(), lutOdd.size()) || rs != lutOdd.size()) {
        return 20;
    }
    if (!ReadFile("./input/lut_intt_even_stacked.bin", rs, lutInttEven.data(), lutInttEven.size()) ||
        rs != lutInttEven.size()) {
        return 21;
    }
    if (!ReadFile("./input/lut_intt_odd_stacked.bin", rs, lutInttOdd.data(), lutInttOdd.size()) ||
        rs != lutInttOdd.size()) {
        return 21;
    }

    std::fprintf(stderr, "[kem-decaps] Phase-D Decrypt (sim_2session=%d)\n",
                 ascendc::SimHostDecapsUse2Session() ? 1 : 0);
    const int dRc =
        RunKemDecapsPhaseD(dk.data(), c.data(), lutEven.data(), lutOdd.data(), lutInttEven.data(), lutInttOdd.data(),
                           m.data());
    if (dRc != 0) {
        return dRc;
    }
    (void)WriteFile("./output/m_prime.bin", m.data(), m.size());

    // 行 1–4 切片：Host 偏移（不另开 launch）
    const uint8_t *ek = dk.data() + F203KemDec::kOffEk;
    const uint8_t *h = dk.data() + F203KemDec::kOffH;
    const uint8_t *z = dk.data() + F203KemDec::kOffZ;

#ifndef ASCENDC_CPU_DEBUG
    if (ascendc::SimHostDecapsUse2Session()) {
        std::fprintf(stderr, "[kem-decaps] NOTE: decaps_2session 对照路径（非默认）\n");
    }
#endif

    std::fprintf(stderr, "[kem-decaps] Phase-E G+Encrypt+FO\n");
    const int eRc = RunKemDecapsPhaseE(ek, m.data(), h, z, c.data(), K.data());
    if (eRc != 0) {
        return eRc;
    }
    if (!WriteFile("./output/K.bin", K.data(), K.size())) {
        return 5;
    }
    return 0;
}
