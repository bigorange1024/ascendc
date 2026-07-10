#ifndef NTTS_2S1E_TILING_H
#define NTTS_2S1E_TILING_H

/**
 * @file tiling.h
 * @brief 2s1e Tag5T 探针几何与 workspace 字节偏移表（Host/Kernel/golden 共用）。
 *
 * 流水线位置：main 分配 GM、mmad_custom 寻址 ws、gen_data.py 写 bin 尺寸均依赖本头。
 * 语义：host 逻辑 1×s+1×e；S2 偶/奇 LUT 分乘 + 平面 pack → mat_c_planar（无 Gather）。
 * 与 golden 关系：src/dst/t_hat/a_hat/mat_c/s0/lut 文件字节数须与本文件常量一致。
 *
 * dst 布局 [12,256] int32：
 *   [0..3]   ŝ AIV0 块
 *   [4..7]   ŝ AIV1 块（与 0..3 应相同，设备内复制）
 *   [8..9]   ê AIV0
 *   [10..11] ê AIV1
 */
#include <cstddef>
#include <cstdint>

/** Host↔Kernel 传递的 tiling 结构（写入 input/tiling.bin，≤64B）。 */
struct TilingData {
    int32_t tileLength; /**< 多项式长度 n = 256 */
    int32_t kS;         /**< s 向量 poly 数 = 4（ML-KEM-1024 k） */
    /** mixPass：0=生产全链路；1..7=分段/sanity（见 gen_data / IMPLEMENTATION_REFERENCE） */
    int32_t mixPass;
};

namespace tiling {

/* ---------- 多项式与分核几何 ---------- */
constexpr size_t n = 256;           /**< 单 poly 系数个数 */
constexpr size_t halfN = n / 2;     /**< 偶/奇半长 = 128（Stage2 列宽） */
constexpr size_t kS = 4;            /**< ŝ 向量维数 */
constexpr size_t kE = 4;            /**< ê 向量维数（物理 4 行变体） */
constexpr size_t kEPerAiv = kE / 2; /**< 每 AIV 负责 2 个 ê poly */
constexpr size_t kPolysPerAiv = kS; /**< 每 AIV 握完整 ŝ 的 4 poly（poly-batch） */
constexpr size_t kAivBatches = 2;   /**< AIV0 + AIV1 */
constexpr size_t kLimbsPerPoly = 4; /**< RouteA：hh,lh,hl,ll 四 limb 行 */

constexpr size_t lutCols = 512;
constexpr size_t lutPlanarCols = halfN; /**< 平面 LUT 列 = 128 */
constexpr size_t lutStackedRows = 512;

/** S0 行布局：ŝ0[8] | ŝ1[8] | ê0[4] | ê1[4]（每 bank 含 hi+lo 各 k 行） */
constexpr size_t s0RowsPerSBank = 2 * kS;
constexpr size_t s0RowsPerEBank = 2 * kEPerAiv;
constexpr size_t S0_ROW_S0 = 0;
constexpr size_t S0_ROW_S1 = s0RowsPerSBank;
constexpr size_t S0_ROW_E0 = S0_ROW_S1 + s0RowsPerSBank;
constexpr size_t S0_ROW_E1 = S0_ROW_E0 + s0RowsPerEBank;
constexpr size_t mRowsLogic = S0_ROW_E1 + s0RowsPerEBank; /**< 逻辑有效行 32 */
constexpr size_t mRowsPad = 8;                             /**< ws 对齐垫片行 */
constexpr size_t mRows = mRowsLogic + mRowsPad;

/** 平面 mat_c 槽位与 dst 对齐：s_aiv0[0..3] | s_aiv1[4..7] | e_aiv0[8..9] | e_aiv1[10..11] */
constexpr size_t kPlanarSlots = 2 * kS + kE;
constexpr size_t PLANAR_SLOT_S0 = 0;
constexpr size_t PLANAR_SLOT_S1 = kS;
constexpr size_t PLANAR_SLOT_E0 = 2 * kS;
constexpr size_t PLANAR_SLOT_E1 = PLANAR_SLOT_E0 + kEPerAiv;
/** 平面行数：slot×4 limb×2 half = 96 */
constexpr size_t matCPlanarRows = kPlanarSlots * kLimbsPerPoly * 2;

/* ---------- workspace 字节偏移（相对 ws 基址，单位：字节）---------- */
constexpr size_t LUT_EVEN_STACKED = 0;
constexpr size_t LUT_EVEN_TOP = LUT_EVEN_STACKED; /**< even LUT 上半（lo 乘） */
constexpr size_t LUT_EVEN_BOTTOM = LUT_EVEN_STACKED + n * lutPlanarCols;
constexpr size_t LUT_ODD_STACKED = LUT_EVEN_BOTTOM + n * lutPlanarCols;
constexpr size_t LUT_ODD_TOP = LUT_ODD_STACKED;
constexpr size_t LUT_ODD_BOTTOM = LUT_ODD_STACKED + n * lutPlanarCols;
constexpr size_t S0 = LUT_ODD_BOTTOM + n * lutPlanarCols; /**< Stage1 输出 int8 [mRows,n] */

constexpr size_t matCTmpBytes = mRows * halfN * sizeof(int32_t);
constexpr size_t MAT_C_TMP_LO_EVEN = S0 + mRows * n; /**< Cube 四路临时：lo×even/odd + hi×even/odd */
constexpr size_t MAT_C_TMP_LO_ODD = MAT_C_TMP_LO_EVEN + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_EVEN = MAT_C_TMP_LO_ODD + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_ODD = MAT_C_TMP_HI_EVEN + matCTmpBytes;
constexpr size_t MAT_C_PLANAR = MAT_C_TMP_HI_ODD + matCTmpBytes; /**< 平面 mat_c [96,128] int32 */

constexpr size_t MAT_C = MAT_C_PLANAR; /**< 别名：dump/对拍用 */
constexpr size_t matCStackedRows = matCPlanarRows;

constexpr size_t kDstPolys = 2 * kS + kE; /**< dst 总 poly 行 = 12 */
constexpr size_t kHatK = kS;
constexpr size_t kHatKK = kHatK * kHatK; /**< Â 矩阵 poly 数 = 16 */
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

/** host 输入文件字节：逻辑 1s+1e，物理 8 行（4×s 重复 + 4×e 变体） */
constexpr size_t kSrcPolys = kS + kE;
constexpr size_t srcFileBytes = kSrcPolys * n * sizeof(int32_t);
constexpr size_t dstFileBytes = kDstPolys * n * sizeof(int32_t);
constexpr size_t tHatFileBytes = kHatK * n * sizeof(int32_t);
constexpr size_t aHatFileBytes = kHatKK * n * sizeof(int32_t);
constexpr size_t matCFileBytes = matCPlanarRows * halfN * sizeof(int32_t);
constexpr size_t s0FileBytes = mRows * n;
constexpr size_t lutEvenOddFileBytes = lutStackedRows * lutPlanarCols;

} // namespace tiling

/** ByteEncode₁₂：单 poly 384B；polyvec = k×384（行 19–20 ek/sk 输出尺寸） */
namespace byte_encode {
constexpr size_t polyBytes = 384;
constexpr size_t polyVecBytes = tiling::kHatK * polyBytes;
} // namespace byte_encode

#endif
