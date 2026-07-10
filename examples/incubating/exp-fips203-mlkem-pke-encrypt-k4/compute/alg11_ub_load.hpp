/**
 * @file alg11_ub_load.hpp
 * @brief Alg.11：把 γ ROM / 系数行装入 UB，供向量 basemul。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-1024 Encrypt compute 内积段。
 * 与 golden：中间搬运，最终对拍仍为 output/c.bin。
 */

// @probe exp-fips203-mlkem-pke-keygen-k4
// @file compute/alg11_ub_load.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `alg11_ub_load.hpp` 为该子模块组件。 / Component: alg11_ub_load.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: alg11_rom_tables.h, kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。

#pragma once

#include "alg11_rom_tables.h"
#include "kernel_operator.h"

namespace alg11_ub_load {

/** GM ROM → UB 连续 DataCopy（count 个 int32，须 32B 对齐；128/256 满足）。 */
__aicore__ inline void copy_rom_int32_ub(AscendC::LocalTensor<int32_t> &dst, __gm__ const int32_t *rom, int32_t count)
{
    AscendC::GlobalTensor<int32_t> gm;
    gm.SetGlobalBuffer(const_cast<__gm__ int32_t *>(rom), count);
    AscendC::DataCopy(dst, gm, count);
}

}  // namespace alg11_ub_load
