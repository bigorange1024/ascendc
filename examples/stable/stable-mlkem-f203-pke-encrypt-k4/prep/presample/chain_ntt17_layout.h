// @probe stable-mlkem-f203-pke-keygen-k4
// @file prep/presample/chain_ntt17_layout.h
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样与 NTT17 链入口；从 seed 派生设备侧中间量供 alg7/alg8/ahat。 / Presample + Keccak/PRF device vector entry. 本文件 `chain_ntt17_layout.h` 为该子模块组件。 / Component: chain_ntt17_layout.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: cstddef, cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


#pragma once

#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength;
    int32_t kS;
    int32_t mixPass;
};

namespace chain_ntt17 {

constexpr size_t n = 256;
constexpr size_t halfN = n / 2;
constexpr size_t kS = 4;
constexpr size_t kE = 4;
constexpr size_t kDstPolys = 2 * kS + kE;
constexpr size_t kHatK = kS;
constexpr size_t kHatKK = kHatK * kHatK;
constexpr size_t lutStackedRows = 512;
constexpr size_t lutPlanarCols = halfN;
constexpr size_t mRows = 8 + 2 * (2 * kS) + 2 * (2 * (kE / 2));
constexpr size_t kPlanarSlots = 2 * kS + kE;
constexpr size_t kLimbsPerPoly = 4;
constexpr size_t matCPlanarRows = kPlanarSlots * kLimbsPerPoly * 2;

constexpr size_t LUT_EVEN_STACKED = 0;
constexpr size_t LUT_EVEN_BOTTOM = LUT_EVEN_STACKED + n * lutPlanarCols;
constexpr size_t LUT_ODD_STACKED = LUT_EVEN_BOTTOM + n * lutPlanarCols;
constexpr size_t LUT_ODD_BOTTOM = LUT_ODD_STACKED + n * lutPlanarCols;
constexpr size_t S0 = LUT_ODD_BOTTOM + n * lutPlanarCols;
constexpr size_t matCTmpBytes = mRows * halfN * sizeof(int32_t);
constexpr size_t MAT_C_TMP_LO_EVEN = S0 + mRows * n;
constexpr size_t MAT_C_TMP_LO_ODD = MAT_C_TMP_LO_EVEN + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_EVEN = MAT_C_TMP_LO_ODD + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_ODD = MAT_C_TMP_HI_EVEN + matCTmpBytes;
constexpr size_t MAT_C_PLANAR = MAT_C_TMP_HI_ODD + matCTmpBytes;

constexpr size_t wssize = MAT_C_PLANAR + matCPlanarRows * halfN * sizeof(int32_t);
constexpr size_t dstFileBytes = kDstPolys * n * sizeof(int32_t);
constexpr size_t tHatFileBytes = kHatK * n * sizeof(int32_t);
constexpr size_t aHatFileBytes = kHatKK * n * sizeof(int32_t);
constexpr size_t matCFileBytes = matCPlanarRows * halfN * sizeof(int32_t);
constexpr size_t s0FileBytes = mRows * n;
constexpr size_t lutEvenOddFileBytes = lutStackedRows * lutPlanarCols;
constexpr size_t kEkSkBytes = kHatK * 384;

}  // namespace chain_ntt17
