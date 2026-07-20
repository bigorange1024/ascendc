/**
 * @file f203_kem_dec_pack_fo_entry.cpp
 * @brief Phase-E 尾：Compress/ByteEncode → c'，同核 KemDecFo → K。
 *
 * CPU：替代 `f203_encrypt_alg14_pack`。
 * SIM 过渡：亦可单独 launch 本核（若 l18_l19 已内联 pack，则仅跑 FO 分支——见 fo-only 入口）。
 */
#include "f203_tail_pack_ops.hpp"
#include "f203_encrypt_tail_layout.h"
#include "f203_kem_dec_fo.hpp"
#include "kernel_operator.h"

using namespace AscendC;

/** pack + FO（CPU 末核 / SIM 若跳过内联 pack 时）。 */
extern "C" __global__ __aicore__ void f203_kem_dec_pack_fo(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cPrimeGm, GM_ADDR cInGm,
                                                           GM_ADDR zGm, GM_ADDR KprimeGm, GM_ADDR KoutGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() != 0) {
        return;
    }

    GlobalTensor<int32_t> gmU;
    GlobalTensor<int32_t> gmV;
    GlobalTensor<uint8_t> gmC;
    gmU.SetGlobalBuffer((__gm__ int32_t *)uGm, f203_tail::kPackK * f203_tail::kPackN);
    gmV.SetGlobalBuffer((__gm__ int32_t *)vGm, f203_tail::kPackN);
    gmC.SetGlobalBuffer((__gm__ uint8_t *)cPrimeGm, F203_TAIL_C_BYTES);

    for (uint32_t p = 0; p < f203_tail::kPackK; ++p) {
        f203_tail::pack_one_u_poly_d11(gmU, gmC, p);
    }
    f203_tail::pack_v_poly_d5(gmV, gmC);

    F203KemDec::KemDecFo(reinterpret_cast<__gm__ uint8_t *>(cInGm), reinterpret_cast<__gm__ uint8_t *>(cPrimeGm),
                         reinterpret_cast<__gm__ uint8_t *>(zGm), reinterpret_cast<__gm__ uint8_t *>(KprimeGm),
                         reinterpret_cast<__gm__ uint8_t *>(KoutGm));
}

/** SIM 过渡：c' 已由 l18_l19 写出，本核只做 FO。 */
extern "C" __global__ __aicore__ void f203_kem_dec_fo_only(GM_ADDR cPrimeGm, GM_ADDR cInGm, GM_ADDR zGm,
                                                           GM_ADDR KprimeGm, GM_ADDR KoutGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() != 0) {
        return;
    }
    F203KemDec::KemDecFo(reinterpret_cast<__gm__ uint8_t *>(cInGm), reinterpret_cast<__gm__ uint8_t *>(cPrimeGm),
                         reinterpret_cast<__gm__ uint8_t *>(zGm), reinterpret_cast<__gm__ uint8_t *>(KprimeGm),
                         reinterpret_cast<__gm__ uint8_t *>(KoutGm));
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_kem_dec_pack_fo_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uGm, uint8_t *vGm,
                                        uint8_t *cPrimeGm, uint8_t *cInGm, uint8_t *zGm, uint8_t *KprimeGm,
                                        uint8_t *KoutGm)
{
    f203_kem_dec_pack_fo<<<blockDim, l2ctrl, stream>>>(uGm, vGm, cPrimeGm, cInGm, zGm, KprimeGm, KoutGm);
}

extern "C" void f203_kem_dec_fo_only_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *cPrimeGm,
                                        uint8_t *cInGm, uint8_t *zGm, uint8_t *KprimeGm, uint8_t *KoutGm)
{
    f203_kem_dec_fo_only<<<blockDim, l2ctrl, stream>>>(cPrimeGm, cInGm, zGm, KprimeGm, KoutGm);
}
#endif
