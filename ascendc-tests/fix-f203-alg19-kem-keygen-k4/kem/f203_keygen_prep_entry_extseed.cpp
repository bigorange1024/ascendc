/**
 * @file f203_keygen_prep_entry_extseed.cpp
 * @brief 旁路 A（KEM_KG_EXT_SEED，test-only）prep 设备 kernel 统一 entry（注册 f203_keygen_prep）。
 *
 * 背景：仅当 CMake `KEM_KG_EXT_SEED=1` 时编入本 TU 替代 vendor/pke_keygen 的同名 entry，
 *   使 device 吃 host 提供的 kem_seed（d‖z）而非 SHA3(域分离串‖SEED_D) 派生，便于与
 *   liboqs keypair_derand 对拍相同随机字节。注册符号 `f203_keygen_prep` 与签名完全一致，
 *   故 main / launch 调用无需改动；差异仅在核内 d 的来源。
 *
 *   宏关（默认/生产）时本 TU 不参与编译，prep 走 vendored entry（seed_d → device 派生 d）。
 */
#include "f203_a_hat16_config.h"
#include "f203_keygen_prep_ub.hpp"
#if KEM_KG_EXT_SEED
#include "f203_keygen_prep_extseed.hpp"
#endif

extern "C" __global__ __aicore__ void f203_keygen_prep(GM_ADDR seed_d_gm, GM_ADDR a_hat_gm, GM_ADDR prf_out_gm,
                                                       GM_ADDR src_gm, GM_ADDR rho_gm, GM_ADDR x_gm, GM_ADDR lengths_gm,
                                                       GM_ADDR se_workspace_gm, GM_ADDR se_tiling_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    (void)x_gm;
    (void)lengths_gm;
    (void)se_workspace_gm;

#if KEM_KG_EXT_SEED
    // 旁路 A：seed_d_gm 承载 64B kem_seed = d(32)‖z(32)；prep 取前 32B 作 d 注入。
    const __gm__ uint8_t *seedBytes = reinterpret_cast<const __gm__ uint8_t *>(seed_d_gm);
    uint8_t d_ext[32];
    for (uint32_t i = 0U; i < 32U; ++i) {
        d_ext[i] = seedBytes[i];
    }
    F203KeygenPrep::BuildKeygenPrepSinglePipeExtD(d_ext, AscendC::GetBlockIdx(),
                                                  reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                                                  reinterpret_cast<__gm__ uint8_t *>(prf_out_gm),
                                                  reinterpret_cast<__gm__ int32_t *>(src_gm),
                                                  reinterpret_cast<__gm__ uint8_t *>(rho_gm), se_tiling_gm);
#else
    // 宏关：与 vendored entry 行为一致（seed_d → device UB 派生 d）。
    const __gm__ uint32_t *seedPtr = reinterpret_cast<const __gm__ uint32_t *>(seed_d_gm);
    const uint32_t seed_d = seedPtr[0];
    F203KeygenPrep::BuildKeygenPrepSinglePipe(seed_d, AscendC::GetBlockIdx(),
                                              reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                                              reinterpret_cast<__gm__ uint8_t *>(prf_out_gm),
                                              reinterpret_cast<__gm__ int32_t *>(src_gm),
                                              reinterpret_cast<__gm__ uint8_t *>(rho_gm), se_tiling_gm);
#endif
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_keygen_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                    uint8_t *a_hat_gm, uint8_t *prf_out_gm, uint8_t *src_gm, uint8_t *rho_gm,
                                    uint8_t *x_gm, uint8_t *lengths_gm, uint8_t *se_workspace_gm,
                                    uint8_t *se_tiling_gm)
{
    f203_keygen_prep<<<blockDim, l2ctrl, stream>>>(seed_d_gm, a_hat_gm, prf_out_gm, src_gm, rho_gm, x_gm,
                                                   lengths_gm, se_workspace_gm, se_tiling_gm);
}
#endif
