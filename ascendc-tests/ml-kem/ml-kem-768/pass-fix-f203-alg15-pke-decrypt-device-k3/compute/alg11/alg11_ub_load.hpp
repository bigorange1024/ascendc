/**
 * 【文件头】GM ROM → UB 的连续 DataCopy 辅助。
 *
 * 本文件在流水线中的位置：Init 阶段把 γ / Gather / interleave 索引从 __gm__ 拷入 UB。
 * 作用：封装 GlobalTensor + DataCopy，供 alg11_vec::init_rom_luts_ub 调用。
 * 与 golden 关系：仅搬运常量，不参与数值对拍。
 */
#pragma once

#include "alg11_rom_tables.h"
#include "kernel_operator.h"

namespace alg11_ub_load {

/**
 * 将 GM ROM 连续拷贝到 UB LocalTensor。
 * @param dst    目标 UB，int32 LocalTensor
 * @param rom    __gm__ 源指针
 * @param count  元素个数（须 32B 对齐；128/256 满足）
 * 前置：dst 已 Alloc 且容量 ≥ count。
 */
__aicore__ inline void copy_rom_int32_ub(AscendC::LocalTensor<int32_t> &dst, __gm__ const int32_t *rom, int32_t count)
{
    AscendC::GlobalTensor<int32_t> gm;
    gm.SetGlobalBuffer(const_cast<__gm__ int32_t *>(rom), count);
    AscendC::DataCopy(dst, gm, count);
}

}  // namespace alg11_ub_load
