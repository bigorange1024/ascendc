/**
 * @file main_kem_decaps_phase_e.cpp
 * @brief Phase-E-only 入口（调试）：读 input/{ek,m_prime,h,z,c} → output/K.bin。
 *
 * 生产默认走 main_kem_decaps.cpp 全链；本文件仅 `KEM_DECAPS_PHASEE_ONLY=1` / phase_e profile。
 */
#include "data_utils.h"
#include "f203_kem_dec_layout.h"
#include "main_kem_decaps_phase_e_run.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

int32_t main(int32_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    std::vector<uint8_t> ek(F203KemDec::kEkKemBytes);
    std::vector<uint8_t> m(F203KemDec::kMsgBytes);
    std::vector<uint8_t> h(F203KemDec::kHashBytes);
    std::vector<uint8_t> z(F203KemDec::kHashBytes);
    std::vector<uint8_t> c(F203KemDec::kCtBytes);
    std::vector<uint8_t> K(F203KemDec::kSharedSecretBytes);

    size_t rs = 0;
    if ((!ReadFile("./input/ek_kem.bin", rs, ek.data(), ek.size()) || rs != ek.size()) &&
        (!ReadFile("./input/ek_pke.bin", rs, ek.data(), ek.size()) || rs != ek.size())) {
        return 13;
    }
    if (!ReadFile("./input/m_prime.bin", rs, m.data(), m.size()) || rs != m.size()) {
        return 8;
    }
    if (!ReadFile("./input/h.bin", rs, h.data(), h.size()) || rs != h.size()) {
        return 9;
    }
    if (!ReadFile("./input/z.bin", rs, z.data(), z.size()) || rs != z.size()) {
        return 10;
    }
    if (!ReadFile("./input/c.bin", rs, c.data(), c.size()) || rs != c.size()) {
        return 11;
    }

    const int rc = RunKemDecapsPhaseE(ek.data(), m.data(), h.data(), z.data(), c.data(), K.data());
    if (rc != 0) {
        return rc;
    }
    if (!WriteFile("./output/K.bin", K.data(), K.size())) {
        return 5;
    }
    return 0;
}
