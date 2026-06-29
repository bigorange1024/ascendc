#ifndef HAT_LINE18_2S1E_HPP
#define HAT_LINE18_2S1E_HPP

#include "tiling.h"

namespace twos1e {

/** 每 AIV 握完整 ŝ[0..3]：行号即 j。 */
__aicore__ inline uint32_t s_row(uint16_t j)
{
    return static_cast<uint32_t>(j);
}

/** 本 AIV ê 在 ub_ntt 中的 local 行（接在 kS 条 ŝ 后）。 */
__aicore__ inline uint32_t e_local_lp(uint16_t p, uint16_t pBegin)
{
    return static_cast<uint32_t>(tiling::kS) + (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin));
}

__aicore__ inline uint16_t p_begin(int32_t subCoreIdx)
{
    return static_cast<uint16_t>(subCoreIdx) * static_cast<uint16_t>(tiling::kEPerAiv);
}

__aicore__ inline uint16_t p_end(int32_t subCoreIdx)
{
    return static_cast<uint16_t>(subCoreIdx) * static_cast<uint16_t>(tiling::kEPerAiv) +
           static_cast<uint16_t>(tiling::kEPerAiv);
}

} // namespace twos1e

#endif
