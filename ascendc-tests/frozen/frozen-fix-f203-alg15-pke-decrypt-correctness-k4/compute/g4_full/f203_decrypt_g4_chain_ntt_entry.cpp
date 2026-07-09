/**
 * @file f203_decrypt_g4_chain_ntt_entry.cpp
 * @brief G4 **Launch-2**：NTT(u) → su_dot → pad ŵ（与 INTT 分 launch，避免 MIX flag 冲突）。
 */
#include "f203_decrypt_layout.h"
#include "f203_decrypt_ntt_u_impl.hpp"
#include "f203_decrypt_ntt_u_tiling.h"
#include "f203_decrypt_su_dot_impl.hpp"
#include "kernel_operator.h"

#ifdef ASCENDC_CPU_DEBUG
volatile int g_f203_decrypt_g4_chain_ntt_mix_pass = 3;
#endif

extern "C" __global__ __aicore__ void f203_decrypt_g4_chain_ntt(GM_ADDR uGm, GM_ADDR sHatGm, GM_ADDR uHatGm,
                                                                 GM_ADDR wHatGm, GM_ADDR wPaddedGm,
                                                                 GM_ADDR nttWsGm, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t blockIdx = static_cast<int32_t>(AscendC::GetBlockIdx());

    tiling.tileLength = static_cast<int32_t>(::tiling::n);
    tiling.kPolys = static_cast<int32_t>(::tiling::kK);
#ifdef ASCENDC_CPU_DEBUG
    tiling.mixPass = g_f203_decrypt_g4_chain_ntt_mix_pass;
#else
    tiling.mixPass = 3;
#endif

    decrypt_g4::ntt_u_impl(uHatGm, uGm, nttWsGm, tiling);
    AscendC::PipeBarrier<PIPE_ALL>();

    if (!AIC && blockIdx == 0 && AscendC::GetSubBlockIdx() == 0) {
        decrypt_g4::su_dot_impl(sHatGm, uHatGm, wHatGm);
        AscendC::PipeBarrier<PIPE_ALL>();
        decrypt_g4::pad_w_hat_for_intt(wPaddedGm, wHatGm);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}
