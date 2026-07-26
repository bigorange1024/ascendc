// @probe exp-fips203-mlkem-pke-keygen-k3
// @file compute/alg11_ub_load.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `alg11_ub_load.hpp` 为该子模块组件。 / Component: alg11_ub_load.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1184B) + dk_pke.bin (1152B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: alg11_rom_tables.h, kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 行 18 Alg.11 basemul / γ 表与向量管线。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-768（k=3）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/alg11_ub_load.hpp
 */
/**
 * @file alg11_ub_load.hpp
 * @brief GM 侧 Alg.11 ROM 表导入 UB 的薄封装（DataCopy int32 块）。
 *
 * 用途：copy_rom_int32_ub — 将 __gm__ const int32_t* 连续拷贝到 LocalTensor（须 32B 对齐，128/256 满足）。
 *
 * 调用方：hat_alg11_basemul.hpp（γ 切片）、multiply_ntts_vec.hpp::init_rom_luts_ub。
 *
 * 不变量：count 个 int32；调用方负责后续 ALG11_PIPE_MTE2()。
 *
 * Golden：无直接对拍；ROM 内容与 alg11_rom_tables.cpp / alg11_gammas.h 一致。
 *
 * CMake：ALG11_MEM_OPS=1 启用 GM ROM 路径。
 */
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
