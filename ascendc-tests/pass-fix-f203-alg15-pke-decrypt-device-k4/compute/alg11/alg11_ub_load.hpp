/**
 * @file alg11_ub_load.hpp
 * @brief Alg.11：GM ROM 常量表 → UB 的连续 DataCopy 封装。
 *
 * 流水线位置：MultiplyNTTs Init（γ / Gather 索引 / interleave 重排表）。
 * 约束：count 个 int32 须满足 DataCopy 对齐（128/256 满足）；
 * 禁止在 Compute 热路径对 ROM 逐元素 SetValue。
 */
#pragma once

#include "alg11_rom_tables.h"
#include "kernel_operator.h"

namespace alg11_ub_load {

/**
 * 把 __gm__ int32 ROM 连续拷入 dst UB。
 * @param dst 已分配 LocalTensor
 * @param rom GM 常量基址
 * @param count 元素个数（int32）
 */
__aicore__ inline void copy_rom_int32_ub(AscendC::LocalTensor<int32_t> &dst, __gm__ const int32_t *rom, int32_t count)
{
    AscendC::GlobalTensor<int32_t> gm;
    gm.SetGlobalBuffer(const_cast<__gm__ int32_t *>(rom), count);
    AscendC::DataCopy(dst, gm, count);
}

}  // namespace alg11_ub_load
