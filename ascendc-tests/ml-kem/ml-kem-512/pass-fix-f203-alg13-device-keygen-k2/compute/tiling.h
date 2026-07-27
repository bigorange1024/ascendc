/**
 * @file tiling.h
 * @brief 4-poly 紧凑 Tag5T Stage1–3 的 host/device 共享布局与 workspace 偏移表。
 *
 * 流水线位置：main.cpp（读 tiling.bin / 算 GM 偏移）、mmad_custom.cpp（S0/LUT/mat_c 指针）、
 * aiv_func.hpp（poly-batch 切分与平面行号）、gen_data.py（文件字节与形状约定）。
 *
 * 作用：
 *   - 输入 polyvec `src` [4,256] int32；输出 NTT/INTT `dst` [4,256] int32；
 *   - Stage1 紧凑 S0：[HI₄, LO₄] 共 8 行 × 256 列 int8（无插零，真 polyvec4，禁 pad 到 6/8）；
 *   - Stage2 四路 Cube 临时 mat_c_tmp → 平面 mat_c [32,128] int32（每 poly 握有完整 hi+lo limbs）；
 *   - LUT even/odd stacked 与 vec-k4-v2 同构；NTT/INTT 仅由 host 写入不同 bin 切换。
 *
 * 与 golden 关系：`srcFileBytes`/`dstFileBytes`/`matCFileBytes`/`s0FileBytes`/`lutEvenOddFileBytes`
 * 须与 gen_data 写出的 bin 及 verify_result 读取尺寸一致；workspace `wssize` 覆盖 LUT+S0+四临时+平面。
 *
 * 不变量（poly-batch）：`kPolysPerAiv=2`，AIV0 处理 poly 0..1，AIV1 处理 2..3；单 poly 的 HI/LO 同属一核。
 */
#ifndef STAGE123_POLYVEC4_TILING_H
#define STAGE123_POLYVEC4_TILING_H

#include <cstddef>
#include <cstdint>

/**
 * Host↔Device 传递的 tiling 结构（KeyGen 由 main_keygen.cpp 直接填充）。
 * mixPass：0=生产全链；1/2/3/4/5/7 为继承 k4 调试分段，默认不得使用。
 */
struct TilingData {
    int32_t tileLength; /**< n = 256，单 poly 系数个数 */
    int32_t kPolys;     /**< 4，本探针固定 polyvec4 长度 */
    /** 0=生产全链；非 0 仅调试 */
    int32_t mixPass;
};

namespace tiling {

constexpr size_t n = 256;                 /**< 单 poly 系数长度 */
constexpr size_t halfN = n / 2;           /**< 128：偶/奇半长，平面列宽 */
constexpr size_t kK = 4;                  /**< polyvec 条数 */
constexpr size_t kS = 2;                  /**< ŝ poly 数：ML-KEM-512 k=2 */
constexpr size_t kE = 2;                  /**< ê poly 数：ML-KEM-512 k=2 */
constexpr size_t kPolysPerAiv = kK / 2;   /**< 每 AIV 2 条 poly（poly-batch） */
constexpr size_t kEPerAiv = 1;            /**< 行18/ByteEncode 每 AIV 输出 1 行：AIV0=p0，AIV1=p1 */
constexpr size_t kLimbsPerPoly = 4;       /**< RouteA 四 limb：hh/lh/hl/ll */

constexpr size_t lutCols = 512;           /**< 原始 LUT 列数（even+odd 各 256） */
constexpr size_t lutPlanarCols = halfN;   /**< stacked 平面列宽 128 */
constexpr size_t lutStackedRows = 512;    /**< even/odd 各 256 top + 256 bottom */

/** 紧凑 S0：行 0..3 = HI，行 4..7 = LO（无假 poly / ZERO 填充） */
constexpr size_t s0RowsLogic = 2 * kK;
constexpr size_t mRowsLogic = s0RowsLogic;
constexpr size_t mRowsPad = 0;
constexpr size_t mRows = mRowsLogic;

/** 平面 mat_c：每 slot 4 limb × 2 half（lo/hi 半）→ 4×4×2 = 32 行 */
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

constexpr size_t matCStackedRows = matCPlanarRows;
constexpr size_t kDstPolys = kK;
constexpr size_t kHatK = kS;
constexpr size_t kHatKK = kHatK * kHatK;
constexpr size_t dstSOffAiv0 = 0;
constexpr size_t dstSOffAiv1 = kS;       /**< 仅保留调试兼容；生产共享 ŝ 从 dst[0..1] 读 */
constexpr size_t dstEOff = kS;
constexpr size_t dstEOffAiv0 = dstEOff;
constexpr size_t dstEOffAiv1 = dstEOff + 1;

/** 整块 workspace 字节数：平面终点 + 平面本体 */
constexpr size_t wssize = MAT_C_PLANAR + matCPlanarRows * halfN * sizeof(int32_t);

constexpr size_t srcFileBytes = kK * n * sizeof(int32_t);
constexpr size_t dstFileBytes = kK * n * sizeof(int32_t);
constexpr size_t tHatFileBytes = kHatK * n * sizeof(int32_t);
constexpr size_t aHatFileBytes = kHatKK * n * sizeof(int32_t);
constexpr size_t matCFileBytes = matCPlanarRows * halfN * sizeof(int32_t);
constexpr size_t s0FileBytes = mRows * n;
constexpr size_t lutEvenOddFileBytes = lutStackedRows * lutPlanarCols;

/** mixPass=6 调试兼容：Cube sanity 仍按硬件 14 行对齐，不代表 polyvec4 语义补行。 */
constexpr size_t sanityMRows = 16;

} // namespace tiling

namespace byte_encode {
constexpr size_t polyBytes = 384;
constexpr size_t polyVecBytes = tiling::kHatK * polyBytes;
} // namespace byte_encode

#endif
