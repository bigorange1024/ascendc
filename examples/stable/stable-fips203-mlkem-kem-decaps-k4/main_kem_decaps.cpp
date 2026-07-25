/**
 * @file main_kem_decaps.cpp
 * @brief Alg.21 Decaps 全链 Host 入口：dk_kem + c → K（Phase-D Decrypt → Phase-E G+Encrypt+FO）。
 *
 * 本目录为 stable 定型交付 v1（自 incubating 整树复制）（vendored Decrypt+Encrypt + kem）；
 * 行为对齐 device 交付探针；customspec：本目录 *-实现方案-customspec.tex。
 *
 * 流水线（Alg.21 Decaps）：
 *   1) 读 dk_kem、c、Decrypt NTT/INTT LUT；
 *   2) Phase-D：`RunKemDecapsPhaseD` → m'（可写 output/m_prime.bin）；
 *   3) Host 按偏移切 ek/h/z（行 1–4，不另开 launch）；
 *   4) Phase-E：`RunKemDecapsPhaseE` → K → output/K.bin。
 *
 * SIM Host：生产默认 `ASCENDC_SIM_HOST_MODE=decaps_1session`（T2 单库后同 session D→E）；
 * 对照可设 `decaps_2session`（非默认）。
 *
 * 与 golden：run.sh + verify 只验 K I/O；本文件不内嵌对拍。
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

    // Host 侧缓冲：私钥、密文、明文候选、共享秘密
    std::vector<uint8_t> dk(F203KemDec::kDkKemBytes);
    std::vector<uint8_t> c(F203KemDec::kCtBytes);
    std::vector<uint8_t> m(F203KemDec::kMsgBytes);
    std::vector<uint8_t> K(F203KemDec::kSharedSecretBytes);

    // Decrypt 用 LUT（无 ntt_ 前缀）；Encrypt Phase-E 另读 lut_ntt_* / lut_intt_*
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

    // —— Phase-D：K-PKE.Decrypt(dk_pke, c) → m' ——
    std::fprintf(stderr, "[kem-decaps] Phase-D Decrypt (sim_2session=%d)\n",
                 ascendc::SimHostDecapsUse2Session() ? 1 : 0);
    const int dRc =
        RunKemDecapsPhaseD(dk.data(), c.data(), lutEven.data(), lutOdd.data(), lutInttEven.data(), lutInttOdd.data(),
                           m.data());
    if (dRc != 0) {
        return dRc;
    }
    (void)WriteFile("./output/m_prime.bin", m.data(), m.size());

    // Alg.18 行 1–4：dk_kem 切片为指针（零拷贝语义）
    const uint8_t *ek = dk.data() + F203KemDec::kOffEk;
    const uint8_t *h = dk.data() + F203KemDec::kOffH;
    const uint8_t *z = dk.data() + F203KemDec::kOffZ;

#ifndef ASCENDC_CPU_DEBUG
    if (ascendc::SimHostDecapsUse2Session()) {
        std::fprintf(stderr, "[kem-decaps] SIM host mode=decaps_2session（对照；非默认）\n");
    } else {
        std::fprintf(stderr, "[kem-decaps] SIM host mode=decaps_1session（生产默认；T2 单库）\n");
    }
#endif

    // —— Phase-E：G + Encrypt + FO → K ——
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
