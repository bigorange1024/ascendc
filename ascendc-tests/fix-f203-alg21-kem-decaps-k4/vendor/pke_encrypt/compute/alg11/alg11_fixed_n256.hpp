#pragma once

#include "kernel_operator.h"

namespace alg11_fixed_n256 {

/** FIPS ML-KEM n=256 固定：用线性公式填索引，替代 CreateVecIndex+Muls+Adds。 */
__aicore__ inline void load_gather_byte_indices(AscendC::LocalTensor<int32_t> &evenByteIdx,
                                                AscendC::LocalTensor<int32_t> &oddByteIdx, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        evenByteIdx.SetValue(i, i * 8);
        oddByteIdx.SetValue(i, i * 8 + 4);
    }
}

}  // namespace alg11_fixed_n256
