#ifndef F203_ENCRYPT_L18_L19_TILING_H
#define F203_ENCRYPT_L18_L19_TILING_H

/**
 * @file f203_l18_l19_tiling.h
 * @brief Alg.14 compute 探针 GM/workspace 尺寸与 INTT k=8 几何。
 *
 * 关键常量：
 *   kK=4           — NTT(y) 与 Â 列数
 *   kP=5           — 内积输出 uTr（û[0..3] + tr̂[4]）
 *   kInttBatch=8   — INTT pad（行 5–7 填零，复用 stage123 polyvec8）
 *   kInttPolysPerAiv=4 — 每 AIV INTT S1 行数
 *
 * workspace：单 launch 内 NTT(k=4) 与 INTT(k=8) 串行复用；INTT 阶段 S0 需 16 行。
 */
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength; /**< n = 256 */
    int32_t kPolys;     /**< 4 */
    int32_t mixPass;    /**< 保留；可行性核固定全链 */
};

namespace tiling {

constexpr size_t n = 256;       /**< 单 poly 系数个数 */
constexpr size_t halfN = n / 2; /**< 128：偶/奇半平面列数 */
/** NTT(y) 与内积 Âᵀ∘ŷ 的 k */
constexpr size_t kK = 4;
/** 行 19/21 统一 INTT batch（pad uTr[5]→8，复用 stage123 k=8 几何） */
constexpr size_t kInttBatch = 8;
constexpr size_t kInttPolysPerAiv = kInttBatch / 2; /**< 每 AIV 4 行 */
/** 内积有意义输出行：û[0..3] + tr̂ */
constexpr size_t kP = 5;

constexpr size_t kPolysPerAiv = kK / 2; /**< NTT 段每 AIV 2 poly */
constexpr size_t kLimbsPerPoly = 4;     /**< RouteA 四 limb 行 */

constexpr size_t lutCols = 512;                    /**< 右 LUT 全宽（偶+奇各 256） */
constexpr size_t lutPlanarCols = halfN;            /**< stacked 平面列 = 128 */
constexpr size_t lutStackedRows = 512;             /**< even/odd 各 256×2 半 */
constexpr size_t lutEvenOddBytes = lutStackedRows * lutPlanarCols; /**< 单侧 stacked 字节（按 int8 计元素） */

/** NTT(y) Stage2 MMAD 行数（k=4 → S0 8 行） */
constexpr size_t nttMRowsLogic = 2 * kK;
/** INTT(uTr pad 8) Stage2 MMAD 行数（S0 16 行） */
constexpr size_t inttMRowsLogic = 2 * kInttBatch;

/** workspace S0 / mat_c 按 INTT k=8 分配（NTT 段仅用前 8 行，余行 memset 0） */
constexpr size_t s0RowsLogic = inttMRowsLogic;
constexpr size_t mRowsLogic = inttMRowsLogic;
constexpr size_t mRows = mRowsLogic;

constexpr size_t kPlanarSlots = kInttBatch; /**< 平面 slot 数 = 8 */
constexpr size_t matCPlanarRows = kPlanarSlots * kLimbsPerPoly * 2;

/** —— workspace 字节偏移（int8 视角；LUT 为 int8，S0 后接 int32 临时）—— */
constexpr size_t LUT_NTT_EVEN_STACKED = 0; /**< NTT even stacked 起点 */
constexpr size_t LUT_NTT_EVEN_TOP = LUT_NTT_EVEN_STACKED;
constexpr size_t LUT_NTT_EVEN_BOTTOM = LUT_NTT_EVEN_STACKED + n * lutPlanarCols;
constexpr size_t LUT_NTT_ODD_STACKED = LUT_NTT_EVEN_BOTTOM + n * lutPlanarCols;
constexpr size_t LUT_NTT_ODD_TOP = LUT_NTT_ODD_STACKED;
constexpr size_t LUT_NTT_ODD_BOTTOM = LUT_NTT_ODD_STACKED + n * lutPlanarCols;
constexpr size_t S0 = LUT_NTT_ODD_BOTTOM + n * lutPlanarCols; /**< Stage1 输出 int8 */

/** 与 stage123 aic/aiv 源文件兼容的 NTT LUT 别名 */
constexpr size_t LUT_EVEN_STACKED = LUT_NTT_EVEN_STACKED;
constexpr size_t LUT_EVEN_TOP = LUT_NTT_EVEN_TOP;
constexpr size_t LUT_EVEN_BOTTOM = LUT_NTT_EVEN_BOTTOM;
constexpr size_t LUT_ODD_STACKED = LUT_NTT_ODD_STACKED;
constexpr size_t LUT_ODD_TOP = LUT_NTT_ODD_TOP;
constexpr size_t LUT_ODD_BOTTOM = LUT_NTT_ODD_BOTTOM;

constexpr size_t matCTmpBytes = mRows * halfN * sizeof(int32_t); /**< 单路 Cube 临时 */
constexpr size_t MAT_C_TMP_LO_EVEN = S0 + mRows * n;
constexpr size_t MAT_C_TMP_LO_ODD = MAT_C_TMP_LO_EVEN + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_EVEN = MAT_C_TMP_LO_ODD + matCTmpBytes;
constexpr size_t MAT_C_TMP_HI_ODD = MAT_C_TMP_HI_EVEN + matCTmpBytes;
constexpr size_t MAT_C_PLANAR = MAT_C_TMP_HI_ODD + matCTmpBytes; /**< 平面 mat_c */

constexpr size_t MAT_C = MAT_C_PLANAR;

constexpr size_t wsCoreBytes = MAT_C_PLANAR + matCPlanarRows * halfN * sizeof(int32_t);
constexpr size_t LUT_INTT_EVEN_STACKED = wsCoreBytes; /**< INTT LUT 接在 core 之后 */
constexpr size_t LUT_INTT_ODD_STACKED = LUT_INTT_EVEN_STACKED + lutEvenOddBytes;
constexpr size_t wssize = LUT_INTT_ODD_STACKED + lutEvenOddBytes; /**< 总 workspace */

/** host 文件尺寸（与 gen_data / main ReadFile 一致） */
constexpr size_t yFileBytes = kK * n * sizeof(int32_t);
constexpr size_t yHatFileBytes = yFileBytes;
constexpr size_t aHatFileBytes = kK * kK * n * sizeof(int32_t);
constexpr size_t uNttFileBytes = kK * n * sizeof(int32_t);
constexpr size_t uTrFileBytes = kP * n * sizeof(int32_t);
constexpr size_t e1FileBytes = kK * n * sizeof(int32_t);
constexpr size_t e2FileBytes = n * sizeof(int32_t);
constexpr size_t uFileBytes = kK * n * sizeof(int32_t);
constexpr size_t vFileBytes = n * sizeof(int32_t);

} // namespace tiling

#endif
