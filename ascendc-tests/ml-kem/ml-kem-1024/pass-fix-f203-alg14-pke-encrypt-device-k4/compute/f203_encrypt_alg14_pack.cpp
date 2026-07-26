/**
 * @file f203_encrypt_alg14_pack.cpp
 * @brief Alg.14 行 22–24：u/v → c（纯 pack；μ 已在 Launch 1 折叠进 e₂）。
 *
 * 流水线：CPU 路径独立 AIV_ONLY launch；SIM 生产路径已内联至 f203_encrypt_l18_l19 尾部。
 * Golden I/O：u.bin + v.bin → c.bin（1568B = c₁‖c₂）；设备例程见 f203_tail_pack_ops.hpp。
 */
#include "f203_encrypt_tail_layout.h"
#include "compute/f203_tail_pack_ops.hpp"
#include "kernel_operator.h"

using namespace AscendC;

/**
 * AIV_ONLY：仅 block0 执行；依次 Compress₁₁+ByteEncode₁₁ 四条 u，再 Compress₅+ByteEncode₅(v)。
 * @param uGm [k,N] int32；@param vGm [N] int32；@param cGm 密文字节
 */
extern "C" __global__ __aicore__ void f203_encrypt_alg14_pack(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cGm)
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
    gmC.SetGlobalBuffer((__gm__ uint8_t *)cGm, F203_TAIL_C_BYTES);

    // c₁：每 poly 352B
    for (uint32_t p = 0; p < f203_tail::kPackK; ++p) {
        f203_tail::pack_one_u_poly_d11(gmU, gmC, p);
    }
    // c₂：160B 接在 c₁ 之后
    f203_tail::pack_v_poly_d5(gmV, gmC);
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_encrypt_alg14_pack_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uGm,
                                           uint8_t *vGm, uint8_t *cGm)
{
    f203_encrypt_alg14_pack<<<blockDim, l2ctrl, stream>>>(uGm, vGm, cGm);
}
#endif
