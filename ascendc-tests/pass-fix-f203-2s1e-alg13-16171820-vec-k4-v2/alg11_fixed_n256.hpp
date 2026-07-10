/**
 * @file alg11_fixed_n256.hpp
 * @brief n=256 固定尺寸：Gather 字节索引用线性公式填表（替代 CreateVecIndex+Muls+Adds）。
 *
 * 用途：ALG11_VEC_OPTS=1 时 load_gather_byte_indices — even[i]=i*8, odd[i]=i*8+4。
 *
 * 调用方：multiply_ntts_vec.hpp（仅 ALG11_VEC_OPTS=1 编译进）。
 *
 * 不变量：pairCount≤128；索引与 B2 Gather 解交错布局一致。
 *
 * Golden：无；属微优化，数学与 legacy 索引等价。
 * 函数体：load_gather_byte_indices 逐 i 写 even=8i、odd=8i+4。
 *
 * CMake：ALG11_VEC_OPTS（默认 CMakeLists 为 1）。
 */
#pragma once

#include "kernel_operator.h"

namespace alg11_fixed_n256 {

/**
 * FIPS ML-KEM n=256 固定：用线性公式填 Gather 字节索引，替代 CreateVecIndex+Muls+Adds。
 * @param evenByteIdx 输出 even[i]=8*i；@param oddByteIdx 输出 odd[i]=8*i+4；@param pairCount≤128
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
