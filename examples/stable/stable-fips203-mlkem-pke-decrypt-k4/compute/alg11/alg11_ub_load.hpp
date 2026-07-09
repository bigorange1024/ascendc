/**
 * @file alg11_ub_load.hpp
 * @brief Alg.11：将 GM ROM 表 DataCopy 进 UB（Decrypt su_dot Init 阶段）。
 *
 * 禁止在 Compute 热路径用 SetValue 填 γ / Gather 索引；须 Init 一次 copy_rom_int32_ub。
 * count 为 int32 个数，须满足 32B 对齐（128/256 元素满足）。
 */
#pragma once

#include "alg11_rom_tables.h"
#include "kernel_operator.h"

namespace alg11_ub_load {

/**
 * GM ROM → UB 连续 DataCopy。
 * @param dst   已分配 LocalTensor；@param rom __gm__ 常量表首址；@param count 元素个数
 */
__aicore__ inline void copy_rom_int32_ub(AscendC::LocalTensor<int32_t> &dst, __gm__ const int32_t *rom, int32_t count)
{
    AscendC::GlobalTensor<int32_t> gm;
    gm.SetGlobalBuffer(const_cast<__gm__ int32_t *>(rom), count);
    AscendC::DataCopy(dst, gm, count);
}

}  // namespace alg11_ub_load
