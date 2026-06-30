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
