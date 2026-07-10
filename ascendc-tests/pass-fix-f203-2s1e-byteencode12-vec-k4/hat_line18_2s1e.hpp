#ifndef HAT_LINE18_2S1E_HPP
#define HAT_LINE18_2S1E_HPP

/**
 * @file hat_line18_2s1e.hpp
 * @brief 2s1e 分核几何辅助：ŝ 行号、ê 本地行、本 AIV 的 poly 批 [pBegin, pEnd)。
 *
 * 流水线位置：设备侧索引工具；AivByteEncode12Only 用其划分 2×AIV 各编码 2 poly。
 * 与 golden 关系：仅决定本核处理哪些 poly 下标；编码 I/O 仍对拍全量 ek/sk golden。
 * 作用：与 tiling::kS / kEPerAiv 一致的行/批边界计算。
 */

#include "tiling.h"

namespace twos1e {

/**
 * 每 AIV 握完整 ŝ[0..3]：行号即 j。
 * @param j poly 下标 0..3
 * @return ub_ntt 中 ŝ 的行号（等于 j）
 */
__aicore__ inline uint32_t s_row(uint16_t j)
{
    return static_cast<uint32_t>(j);
}

/**
 * 本 AIV ê 在 ub_ntt 中的 local 行（接在 kS 条 ŝ 后）。
 * @param p      全局 poly 下标
 * @param pBegin 本 AIV 批起点
 * @return local 行 = kS + (p - pBegin)
 */
__aicore__ inline uint32_t e_local_lp(uint16_t p, uint16_t pBegin)
{
    return static_cast<uint32_t>(tiling::kS) + (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin));
}

/**
 * 本 AIV 负责的 poly 批起点。
 * @param subCoreIdx AIV 子核号 0 或 1
 * @return pBegin = subCoreIdx * kEPerAiv（0 或 2）
 */
__aicore__ inline uint16_t p_begin(int32_t subCoreIdx)
{
    return static_cast<uint16_t>(subCoreIdx) * static_cast<uint16_t>(tiling::kEPerAiv);
}

/**
 * 本 AIV 负责的 poly 批终点（开区间）。
 * @param subCoreIdx AIV 子核号 0 或 1
 * @return pEnd = pBegin + kEPerAiv（2 或 4）
 */
__aicore__ inline uint16_t p_end(int32_t subCoreIdx)
{
    return static_cast<uint16_t>(subCoreIdx) * static_cast<uint16_t>(tiling::kEPerAiv) +
           static_cast<uint16_t>(tiling::kEPerAiv);
}

} // namespace twos1e

#endif
