// @probe stable-fips203-mlkem-pke-keygen-k4
// @file compute/tiling.h
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `tiling.h` 为该子模块组件。 / Component: tiling.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: cstddef, cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


#ifndef NTTS_2S1E_TILING_H
#define NTTS_2S1E_TILING_H

/**
 * @file tiling.h
 * @brief 2s1e NTT：host 1×s+1×e；S2 偶/奇 LUT 分乘 + 平面 pack → mat_c_planar（无 Gather）。
 *
 * dst 布局 [12,256]：
 *   [0..3]   ŝ AIV0 块
 *   [4..7]   ŝ AIV1 块（与 0..3 应相同）
 *   [8..9]   ê AIV0
 *   [10..11] ê AIV1
 */
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength; /**< n = 256 */
    int32_t kS;         /**< s 向量 poly 数 = 4 */
    /** 0=S1+S2+S3+行18+19–20；1=仅 S1；2=仅 S2；3=仅 S3；4=行18+19–20；5=S1+S2+S3 dump；6=MMAD sanity（见 gen_mmad_sanity_data.py） */
    int32_t mixPass;
};

namespace tiling {

constexpr size_t n = 256;
constexpr size_t halfN = n / 2;
constexpr size_t kS = 4;
constexpr size_t kE = 4;
constexpr size_t kEPerAiv = kE / 2;
constexpr size_t kPolysPerAiv = kS;
constexpr size_t kAivBatches = 2;
constexpr size_t kLimbsPerPoly = 4;

constexpr size_t lutCols = 512;
constexpr size_t lutPlanarCols = halfN;
constexpr size_t lutStackedRows = 512;

/** S0 行：ŝ0[8] | ŝ1[8] | ê0[4] | ê1[4] */
constexpr size_t s0RowsPerSBank = 2 * kS;
constexpr size_t s0RowsPerEBank = 2 * kEPerAiv;
constexpr size_t S0_ROW_S0 = 0;
constexpr size_t S0_ROW_S1 = s0RowsPerSBank;
constexpr size_t S0_ROW_E0 = S0_ROW_S1 + s0RowsPerSBank;
constexpr size_t S0_ROW_E1 = S0_ROW_E0 + s0RowsPerEBank;
constexpr size_t mRowsLogic = S0_ROW_E1 + s0RowsPerEBank;
constexpr size_t mRowsPad = 8;
constexpr size_t mRows = mRowsLogic + mRowsPad;

/** 平面 mat_c 槽位与 dst 对齐：s_aiv0[0..3] | s_aiv1[4..7] | e_aiv0[8..9] | e_aiv1[10..11] */
constexpr size_t kPlanarSlots = 2 * kS + kE;
constexpr size_t PLANAR_SLOT_S0 = 0;
constexpr size_t PLANAR_SLOT_S1 = kS;
constexpr size_t PLANAR_SLOT_E0 = 2 * kS;
constexpr size_t PLANAR_SLOT_E1 = PLANAR_SLOT_E0 + kEPerAiv;
constexpr size_t matCPlanarRows = kPlanarSlots * kLimbsPerPoly * 2;

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

constexpr size_t MAT_C = MAT_C_PLANAR;
constexpr size_t matCStackedRows = matCPlanarRows;

constexpr size_t kDstPolys = 2 * kS + kE;
constexpr size_t kHatK = kS;
constexpr size_t kHatKK = kHatK * kHatK;
constexpr size_t dstSOffAiv0 = 0;
constexpr size_t dstSOffAiv1 = kS;
constexpr size_t dstEOff = 2 * kS;
constexpr size_t dstEOffAiv0 = dstEOff;
constexpr size_t dstEOffAiv1 = dstEOff + kEPerAiv;

constexpr size_t wssize = MAT_C_PLANAR + matCPlanarRows * halfN * sizeof(int32_t);

/** mixPass=6：与 tag5t planar-s12 同维 AicMmad(16,256,128) 单路 sanity */
constexpr size_t sanityMRows = 16;
constexpr size_t sanityS0Bytes = sanityMRows * n;
constexpr size_t sanityMatCTmpBytes = sanityMRows * halfN * sizeof(int32_t);

/** host 输入：逻辑 1s+1e，物理 8 行（4×s 重复 + 4×e 变体） */
constexpr size_t kSrcPolys = kS + kE;
constexpr size_t srcFileBytes = kSrcPolys * n * sizeof(int32_t);
constexpr size_t dstFileBytes = kDstPolys * n * sizeof(int32_t);
constexpr size_t tHatFileBytes = kHatK * n * sizeof(int32_t);
constexpr size_t aHatFileBytes = kHatKK * n * sizeof(int32_t);
constexpr size_t matCFileBytes = matCPlanarRows * halfN * sizeof(int32_t);
constexpr size_t s0FileBytes = mRows * n;
constexpr size_t lutEvenOddFileBytes = lutStackedRows * lutPlanarCols;

} // namespace tiling

namespace byte_encode {
constexpr size_t polyBytes = 384;
constexpr size_t polyVecBytes = tiling::kHatK * polyBytes;
} // namespace byte_encode

#endif
