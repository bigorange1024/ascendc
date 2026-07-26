/**
 * @file tiling.h
 * @brief 8-poly 紧凑 Tag5T Stage1–3 的 host/device 共享布局与 workspace 偏移表。
 *
 * 流水线位置：main.cpp（读 tiling.bin / 算 GM 偏移）、mmad_custom.cpp（S0/LUT/mat_c 指针）、
 * aiv_func.hpp（poly-batch 切分与平面行号）、gen_data.py（文件字节与形状约定）。
 *
 * 作用：
 *   - 输入 polyvec `src` [8,256] int32；输出 NTT/INTT `dst` [8,256] int32；
 *   - Stage1 紧凑 S0：[HI₈, LO₈] 共 16 行 × 256 列 int8（无插零，相对 12-poly 的 Tag5T 变体）；
 *   - Stage2 四路 Cube 临时 mat_c_tmp → 平面 mat_c [64,128] int32（每 poly 握有完整 hi+lo limbs）；
 *   - LUT even/odd stacked 与 vec-k4-v2 同构；NTT/INTT 仅由 host 写入不同 bin 切换。
 *
 * 与 golden 关系：`srcFileBytes`/`dstFileBytes`/`matCFileBytes`/`s0FileBytes`/`lutEvenOddFileBytes`
 * 须与 gen_data 写出的 bin 及 verify_result 读取尺寸一致；workspace `wssize` 覆盖 LUT+S0+四临时+平面。
 *
 * 不变量（poly-batch）：`kPolysPerAiv=4`，AIV0 处理 poly 0..3，AIV1 处理 4..7；单 poly 的 HI/LO 同属一核。
 */
#ifndef STAGE123_POLYVEC8_TILING_H
#define STAGE123_POLYVEC8_TILING_H

#include <cstddef>
#include <cstdint>

/**
 * Host↔Device 传递的 tiling 结构（写入 input/tiling.bin，前 12 字节有效，缓冲 64B）。
 * mixPass：0=仅 S1；1=仅 S2；2=仅 S3；3=S1+S2+S3（生产默认）。
 */
struct TilingData {
    int32_t tileLength; /**< n = 256，单 poly 系数个数 */
    int32_t kPolys;     /**< 8，本探针固定 polyvec 长度 */
    /** 0=仅 S1；1=仅 S2；2=仅 S3；3=S1+S2+S3（默认） */
    int32_t mixPass;
};

namespace tiling {

constexpr size_t n = 256;                 /**< 单 poly 系数长度 */
constexpr size_t halfN = n / 2;           /**< 128：偶/奇半长，平面列宽 */
constexpr size_t kK = 8;                  /**< polyvec 条数 */
constexpr size_t kPolysPerAiv = kK / 2;   /**< 每 AIV 4 条 poly（poly-batch） */
constexpr size_t kLimbsPerPoly = 4;       /**< RouteA 四 limb：hh/lh/hl/ll */

constexpr size_t lutCols = 512;           /**< 原始 LUT 列数（even+odd 各 256） */
constexpr size_t lutPlanarCols = halfN;   /**< stacked 平面列宽 128 */
constexpr size_t lutStackedRows = 512;    /**< even/odd 各 256 top + 256 bottom */

/** 紧凑 S0：行 0..7 = HI，行 8..15 = LO（无 ZERO 填充） */
constexpr size_t s0RowsLogic = 2 * kK;
constexpr size_t mRowsLogic = s0RowsLogic;
constexpr size_t mRowsPad = 0;
constexpr size_t mRows = mRowsLogic;

/** 平面 mat_c：每 slot 4 limb × 2 half（lo/hi 半）→ 8×4×2 = 64 行 */
constexpr size_t kPlanarSlots = kK;
constexpr size_t matCPlanarRows = kPlanarSlots * kLimbsPerPoly * 2;

/** workspace 内 LUT / S0 / 四路 Cube 临时 / 平面 mat_c 的字节偏移（相对 ws 基址） */
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

/** 平面 mat_c 别名（Stage3 / dump 入口） */
constexpr size_t MAT_C = MAT_C_PLANAR;

/** 整块 workspace 字节数：平面终点 + 平面本体 */
constexpr size_t wssize = MAT_C_PLANAR + matCPlanarRows * halfN * sizeof(int32_t);

constexpr size_t srcFileBytes = kK * n * sizeof(int32_t);
constexpr size_t dstFileBytes = kK * n * sizeof(int32_t);
constexpr size_t matCFileBytes = matCPlanarRows * halfN * sizeof(int32_t);
constexpr size_t s0FileBytes = mRows * n;
constexpr size_t lutEvenOddFileBytes = lutStackedRows * lutPlanarCols;

} // namespace tiling

#endif
