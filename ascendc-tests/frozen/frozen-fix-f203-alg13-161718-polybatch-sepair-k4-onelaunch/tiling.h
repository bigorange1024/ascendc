/**
 * fix-f203-alg13-161718-polybatch-sepair-k4 — poly-batch NTT + se_pair 行 16–17–18。
 * Stage2：偶/奇 LUT 分乘 + AIV 平面 pack → mat_c_planar [64,128]（无 Stage3 Gather）。
 */
#ifndef __TIILING_H__
#define __TIILING_H__
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength;
    int32_t kPolys;
    /** 0=S1+S2+S3+行18+19–20；1=S1；2=S2；3=S3；4=行18+19–20；5=S1+S2+S3；6=S1+S2；7=仅19–20 */
    int32_t mixPass;
};

namespace tiling {
constexpr size_t n = 256;
constexpr size_t halfN = n / 2;
constexpr size_t kPolys = 8;
constexpr size_t kPolysPerAiv = 4;
constexpr size_t kAivBatches = kPolys / kPolysPerAiv;
constexpr size_t kHatK = 4;
constexpr size_t kHatKK = kHatK * kHatK;
constexpr size_t kLimbsPerPoly = 4;
constexpr size_t lutCols = 512;
constexpr size_t lutPlanarCols = halfN;
constexpr size_t lutStackedRows = 512;
constexpr size_t mRows = 2 * kPolys;
constexpr size_t s0RowsPerAiv = mRows / kAivBatches;
constexpr size_t matCPlanarRows = kPolys * kLimbsPerPoly * 2;

constexpr size_t LUT_EVEN_STACKED = 0;
constexpr size_t LUT_EVEN_TOP = LUT_EVEN_STACKED;
constexpr size_t LUT_EVEN_BOTTOM = LUT_EVEN_STACKED + n * lutPlanarCols;
constexpr size_t LUT_ODD_STACKED = LUT_EVEN_BOTTOM + n * lutPlanarCols;
constexpr size_t LUT_ODD_TOP = LUT_ODD_STACKED;
constexpr size_t LUT_ODD_BOTTOM = LUT_ODD_STACKED + n * lutPlanarCols;
constexpr size_t S0 = LUT_ODD_BOTTOM + n * lutPlanarCols;

constexpr size_t matCTmpBytes = mRows * halfN * sizeof(int32_t);
constexpr size_t MAT_C_TMP_LO_EVEN = S0 + mRows * n;
constexpr size_t MAT_C_TMP_LO_ODD = MAT_C_TMP_LO_EVEN + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_EVEN = MAT_C_TMP_LO_ODD + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_ODD = MAT_C_TMP_HI_EVEN + matCTmpBytes;
constexpr size_t MAT_C_PLANAR = MAT_C_TMP_HI_ODD + matCTmpBytes;

constexpr size_t wssize = MAT_C_PLANAR + matCPlanarRows * halfN * sizeof(int32_t);

/** 行 18：每 AIV 发布本地 kPolysPerAiv 行 ŝ‖ê NTT tile（ws 握手，非算法 dst 往返）。 */
constexpr size_t SHAT_PEER_SLOT_BYTES = kPolysPerAiv * n * sizeof(int32_t);
constexpr size_t SHAT_PEER_BYTES = kAivBatches * SHAT_PEER_SLOT_BYTES;
constexpr size_t SHAT_PEER = wssize;
constexpr size_t wssize_alloc = SHAT_PEER + SHAT_PEER_BYTES;

constexpr size_t lutEvenOddFileBytes = lutStackedRows * lutPlanarCols;
constexpr size_t matCPlanarFileSize = matCPlanarRows * halfN * sizeof(int32_t);

/** 调试 dump 别名 */
constexpr size_t MAT_C = MAT_C_PLANAR;
constexpr size_t matCStackedRows = matCPlanarRows;
constexpr size_t matCHalfElems = mRows * n;
constexpr size_t matCHalfBytes = matCTmpBytes;
constexpr size_t lutHalfCols = lutPlanarCols;
} // namespace tiling

namespace byte_encode {
constexpr size_t polyBytes = 384;
constexpr size_t polyVecBytes = tiling::kHatK * polyBytes;
} // namespace byte_encode

#endif
