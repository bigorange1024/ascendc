/**
 * 【文件头】固定 n=256 时用线性公式填 Gather 字节索引（替代 CreateVecIndex+Muls+Adds）。
 *
 * 本文件在流水线中的位置：ALG11_VEC_OPTS=1 时由 multiply_ntts_vec.hpp 调用。
 * 作用：在 UB 上 SetValue 写出 even=i*8、odd=i*8+4，供 Gather deinterleave。
 * 与 golden 关系：仅布局索引，不改变 Alg.12 代数；须与 ROM 表 / 公式一致。
 */
#pragma once

#include "kernel_operator.h"

namespace alg11_fixed_n256 {

/**
 * 按固定 n=256 公式填充 Gather 字节索引到 UB。
 * @param evenByteIdx  输出偶索引 LocalTensor，长度 pairCount，dtype int32
 * @param oddByteIdx   输出奇索引 LocalTensor，长度 pairCount，dtype int32
 * @param pairCount    对数（本探针为 128）
 * 前置：even/odd 已 Alloc，可写 pairCount 个元素。
 */
__aicore__ inline void load_gather_byte_indices(AscendC::LocalTensor<int32_t> &evenByteIdx,
                                                AscendC::LocalTensor<int32_t> &oddByteIdx, int32_t pairCount)
{
    /* i：第 i 对；偶系数字节偏移 i*8，奇为 i*8+4（int32 宽 4B） */
    for (int32_t i = 0; i < pairCount; ++i) {
        evenByteIdx.SetValue(i, i * 8);
        oddByteIdx.SetValue(i, i * 8 + 4);
    }
}

}  // namespace alg11_fixed_n256
