/**
 * @file f203_decrypt_g4_prep_entry.cpp
 * @brief G4 **Launch-1**：unpack c + decode dk→ŝ（MIX 占位，仅 AIV subcore 0 计算）。
 */
#include "f203_decrypt_decode_impl.hpp"
#include "f203_decrypt_layout.h"
#include "f203_decrypt_unpack_impl.hpp"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void f203_decrypt_g4_prep(GM_ADDR dkGm, GM_ADDR cGm, GM_ADDR uGm, GM_ADDR vGm,
                                                             GM_ADDR sHatGm)
{
#if defined(ASCENDC_CPU_DEBUG)
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (AscendC::GetBlockIdx() != 0) {
        return;
    }
#else
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    if (AscendC::GetSubBlockNum() == 1) {
        return;
    }
    if (AscendC::GetBlockIdx() != 0) {
        return;
    }
#endif

    decrypt_g4::unpack_c_impl(cGm, uGm, vGm);
    AscendC::PipeBarrier<PIPE_ALL>();
    decrypt_g4::decode_s_hat_impl(dkGm, sHatGm);
}
