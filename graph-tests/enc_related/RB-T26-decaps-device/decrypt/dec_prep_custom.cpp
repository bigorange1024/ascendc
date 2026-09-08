/**
 * @file prep_custom.cpp
 * @brief RB-T25 Launch1：prep AIV-only — dk+c → ŝ/u/v。
 *
 * 无 CrossCore；blockDim=1。Host mid-sync 后再 launch NTT。
 */
#include "kernel_operator.h"
#include "decrypt/prep_device_math.hpp"
#include "decrypt/tiling.h"

__aicore__ inline void MarkU32(GM_ADDR base, uint32_t slot, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(base);
    *(p + slot) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * @param dkIn [in] dk_pke[1536]
 * @param cIn  [in] c[1568]
 * @param ws   [out] OFF_DK/C/S_HAT/U/V + TRACE
 */
extern "C" __global__ __aicore__ void dec_prep_custom(GM_ADDR dkIn, GM_ADDR cIn, GM_ADDR ws,
                                                  DecTilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    using namespace dec_tiling;
    rb_t25::PrepUnpackDecrypt(dkIn, cIn, ws);
    MarkU32(ws + OFF_PREP_MARK, 0, MAGIC_PREP_MARK);
    MarkU32(ws + OFF_TRACE, SLOT_PREP_DONE, MAGIC_PREP_DONE);
}
