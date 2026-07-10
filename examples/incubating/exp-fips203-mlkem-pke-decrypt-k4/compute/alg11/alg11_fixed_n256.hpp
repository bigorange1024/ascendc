/**
 * @file alg11_fixed_n256.hpp
 * @brief Alg.11：n=256 固定时用线性公式填 Gather 字节索引（替代 CreateVecIndex 链）。
 *
 * evenByteIdx[i] = i*8；oddByteIdx[i] = i*8+4（int32 按字节寻址时偶/奇半字偏移）。
 * 与 alg11_rom_tables.cpp 中 ALG11_GATHER_*_BYTE_TABLE 公式一致；ALG11_VEC_OPTS==1 时可用。
 */
#pragma once

#include "kernel_operator.h"

namespace alg11_fixed_n256 {

/**
 * 填 Gather 偶/奇字节索引到 UB。
 * @param evenByteIdx / oddByteIdx 输出 LocalTensor；@param pairCount 通常 128
 */
__aicore__ inline void load_gather_byte_indices(AscendC::LocalTensor<int32_t> &evenByteIdx,
                                                AscendC::LocalTensor<int32_t> &oddByteIdx, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        evenByteIdx.SetValue(i, i * 8);
        oddByteIdx.SetValue(i, i * 8 + 4);
    }
}

}  // namespace alg11_fixed_n256
