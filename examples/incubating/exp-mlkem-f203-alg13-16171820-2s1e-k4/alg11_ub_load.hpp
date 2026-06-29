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
