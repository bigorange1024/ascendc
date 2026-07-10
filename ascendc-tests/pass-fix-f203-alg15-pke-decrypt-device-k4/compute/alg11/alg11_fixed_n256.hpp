/**
 * @file alg11_fixed_n256.hpp
 * @brief Alg.11 向量路径：固定 n=256 时用线性公式填 Gather 字节索引。
 *
 * 流水线位置：su_dot / MultiplyNTTs Init；替代 CreateVecIndex+Muls+Adds。
 * 约定：偶系数字节偏移 i*8，奇系数 i*8+4（int32 SoA 交错布局）。
 * 与 ROM 表 gAlg11Gather* 公式一致；ALG11_MEM_OPS=1 时也可 DataCopy ROM。
 */
#pragma once

#include "kernel_operator.h"

namespace alg11_fixed_n256 {

/**
 * 向 UB 写入 even/odd Gather 字节索引（各 pairCount 个 int32）。
 * @param evenByteIdx / oddByteIdx 已分配的 LocalTensor
 * @param pairCount 通常 128（n/2）
 */
__aicore__ inline void load_gather_byte_indices(AscendC::LocalTensor<int32_t> &evenByteIdx,
                                                AscendC::LocalTensor<int32_t> &oddByteIdx, int32_t pairCount)
{
    // 固定 n：索引与系数对序号成正比，无需向量造索引
    for (int32_t i = 0; i < pairCount; ++i) {
        evenByteIdx.SetValue(i, i * 8);
        oddByteIdx.SetValue(i, i * 8 + 4);
    }
}

}  // namespace alg11_fixed_n256
