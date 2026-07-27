/**
 * @file f203_decrypt_ntt_u_tiling.h
 * @brief Alg.15 NTT(u') / INTT(ŵ) 共用 workspace 与几何常量（ML-KEM-512 k=2 poly-batch）。
 *
 * 流水线位置：ntt_u / intt_w / fused 的 nttWsGm、inttWsGm 布局。
 * 语义：Stage1 紧凑 [HI₂‖LO₂] → AIC MMAD → 平面 mat_c → Stage3 merge；
 * AIV 分片为 1+1，Cube m 按硬件对齐但 GM 语义不补假 poly。mixPass=3 为生产全链。
 *
 * 须写 ::tiling::（与 AscendC::tiling 歧义）。偏移单位：元素字节（int8/int32 混用处见注释）。
 * 与 golden：Host stage123_transform 同 LUT / 同平面行语义。
 */
#ifndef F203_DECRYPT_NTT_U_TILING_H
#define F203_DECRYPT_NTT_U_TILING_H

#include <cstddef>
#include <cstdint>

/** Host/设备传递的运行时 tiling（几何仍以本头 constexpr 为准）。 */
struct TilingData {
    int32_t tileLength; /**< n = 256 */
    int32_t kPolys;     /**< k = 2 */
    /** 0=仅 S1；1=仅 S2；2=仅 S3；3=S1+S2+S3（生产默认） */
    int32_t mixPass;
};

namespace tiling {

constexpr size_t n = 256;
constexpr size_t halfN = n / 2;
constexpr size_t kK = 2;
constexpr size_t kPolysPerAiv = 1; /* k=2：AIV0=poly0、AIV1=poly1；每核握完整 hi+lo，非 limbsplit */
constexpr size_t kLimbsPerPoly = 4;

constexpr size_t lutCols = 512;
constexpr size_t lutPlanarCols = halfN;
constexpr size_t lutStackedRows = 512;

constexpr size_t s0RowsLogic = 2 * kK; /* Stage1 输出：HI₂+LO₂，共 4 行 */
constexpr size_t mRowsLogic = s0RowsLogic;
constexpr size_t mRowsPad = 0;
constexpr size_t mRows = mRowsLogic;

constexpr size_t kPlanarSlots = kK;
constexpr size_t matCPlanarRows = kPlanarSlots * kLimbsPerPoly * 2;

/* ---- workspace 字节偏移（相对 ws 基址）---- */
constexpr size_t LUT_EVEN_STACKED = 0;
constexpr size_t LUT_EVEN_TOP = LUT_EVEN_STACKED;
constexpr size_t LUT_EVEN_BOTTOM = LUT_EVEN_STACKED + n * lutPlanarCols;
constexpr size_t LUT_ODD_STACKED = LUT_EVEN_BOTTOM + n * lutPlanarCols;
constexpr size_t LUT_ODD_TOP = LUT_ODD_STACKED;
constexpr size_t LUT_ODD_BOTTOM = LUT_ODD_STACKED + n * lutPlanarCols;
constexpr size_t S0 = LUT_ODD_BOTTOM + n * lutPlanarCols; /* Stage1 紧凑 s0 */

constexpr size_t matCTmpBytes = mRows * halfN * sizeof(int32_t);
constexpr size_t MAT_C_TMP_LO_EVEN = S0 + mRows * n;
constexpr size_t MAT_C_TMP_LO_ODD = MAT_C_TMP_LO_EVEN + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_EVEN = MAT_C_TMP_LO_ODD + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_ODD = MAT_C_TMP_HI_EVEN + matCTmpBytes;
constexpr size_t MAT_C_PLANAR = MAT_C_TMP_HI_ODD + matCTmpBytes;

constexpr size_t MAT_C = MAT_C_PLANAR;

constexpr size_t wssize = MAT_C_PLANAR + matCPlanarRows * halfN * sizeof(int32_t);

/* Host 文件尺寸（gen_data / 对拍夹具） */
constexpr size_t srcFileBytes = kK * n * sizeof(int32_t);
constexpr size_t dstFileBytes = kK * n * sizeof(int32_t);
constexpr size_t matCFileBytes = matCPlanarRows * halfN * sizeof(int32_t);
constexpr size_t s0FileBytes = mRows * n;
constexpr size_t lutEvenOddFileBytes = lutStackedRows * lutPlanarCols;

} // namespace tiling

#endif
