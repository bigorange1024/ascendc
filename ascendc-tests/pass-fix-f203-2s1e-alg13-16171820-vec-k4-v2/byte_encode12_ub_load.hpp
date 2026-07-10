/**
 * @file byte_encode12_ub_load.hpp
 * @brief ByteEncode₁₂ GM ROM → UB 的 DataCopy 薄封装。
 *
 * 流水线位置：byte_encode12_vec.hpp Init/prefetch 路径。
 * 与 golden 关系：无；仅搬运索引表。
 */
#pragma once

#include "byte_encode12_rom_tables.h"
#include "kernel_operator.h"

namespace byte_encode12_ub_load {

/**
 * GM ROM → UB 连续 DataCopy。
 * @param dst UB int32；@param rom __gm__ 表；@param count 元素数（128/256 满足 32B 对齐）
 */
__aicore__ inline void copy_rom_int32_ub(AscendC::LocalTensor<int32_t> &dst, __gm__ const int32_t *rom, int32_t count)
{
    AscendC::GlobalTensor<int32_t> gm;
    gm.SetGlobalBuffer(const_cast<__gm__ int32_t *>(rom), count);
    AscendC::DataCopy(dst, gm, count);
}

} // namespace byte_encode12_ub_load
