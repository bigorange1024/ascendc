// @probe pass-fix-f203-alg13-device-keygen-k4
// @file compute/alg11_fixed_n256.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `alg11_fixed_n256.hpp` 为该子模块组件。 / Component: alg11_fixed_n256.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。

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
 *
 * CMake：ALG11_VEC_OPTS（默认 CMakeLists 为 1）。
 */
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
