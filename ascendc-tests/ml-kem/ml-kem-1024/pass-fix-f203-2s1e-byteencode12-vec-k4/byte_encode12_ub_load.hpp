#pragma once

/**
 * @file byte_encode12_ub_load.hpp
 * @brief ByteEncode₁₂ prefetch：GM ROM int32 表 → UB 连续 DataCopy 辅助。
 *
 * 流水线位置：设备侧工具；由 byte_encode12_vec.hpp 的 prefetch 路径调用，装载 Gather 索引。
 * 与 golden 关系：纯搬运，不改变编码语义；索引内容见 rom_tables。
 * 作用：将 __gm__ const int32 表拷入 LocalTensor，供后续 Gather 使用。
 */

#include "byte_encode12_rom_tables.h"
#include "kernel_operator.h"

namespace byte_encode12_ub_load {

/**
 * GM ROM → UB 连续 DataCopy（count 个 int32，128/256 满足 32B 对齐）。
 * @param dst   UB LocalTensor<int32>，容量 ≥ count
 * @param rom   GM 常量表指针（如 gByteEncode12GatherEvenByteGm）
 * @param count 元素个数（本探针为 128）
 * 前置条件：dst 已分配；count*4 满足 DataCopy 对齐要求。
 */
__aicore__ inline void copy_rom_int32_ub(AscendC::LocalTensor<int32_t> &dst, __gm__ const int32_t *rom, int32_t count)
{
    AscendC::GlobalTensor<int32_t> gm;
    // 将 const GM 指针包装为 GlobalTensor 后整段拷入 UB
    gm.SetGlobalBuffer(const_cast<__gm__ int32_t *>(rom), count);
    AscendC::DataCopy(dst, gm, count);
}

} // namespace byte_encode12_ub_load
