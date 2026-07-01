/**
 * @file f203_kem_kg_finish_entry.cpp
 * @brief Launch-3：Alg.19 KeyGen 尾段（KeyGen_internal：H(ek) + UB 内 z + dk_kem 拼接）。
 */
#include "f203_kem_kg_finish.hpp"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void f203_kem_kg_finish(GM_ADDR seed_d_gm, GM_ADDR ek_pke_gm, GM_ADDR dk_pke_gm,
                                                         GM_ADDR ek_kem_gm, GM_ADDR dk_kem_gm)
{
#if defined(ASCENDC_CPU_DEBUG)
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (AscendC::GetBlockIdx() != 0) {
        return;
    }
#else
    // SIM：MIX 占位释 AIV func_key 名额（对齐 Decrypt prep）
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    if (AscendC::GetSubBlockNum() == 1) {
        return;
    }
    if (AscendC::GetBlockIdx() != 0) {
        return;
    }
#endif
    F203KemKg::KemKgFinishImpl(reinterpret_cast<__gm__ uint8_t *>(seed_d_gm),
                               reinterpret_cast<__gm__ uint8_t *>(ek_pke_gm),
                               reinterpret_cast<__gm__ uint8_t *>(dk_pke_gm),
                               reinterpret_cast<__gm__ uint8_t *>(ek_kem_gm),
                               reinterpret_cast<__gm__ uint8_t *>(dk_kem_gm));
}
