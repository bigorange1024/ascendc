/**
 * @file f203_encrypt_alg14_pack.cpp
 * @brief Alg.14 行 22–24：u/v → c（Compress+ByteEncode；μ 已在前段折叠进 e₂）。
 *
 * 流水线位置：CPU 五 launch 末核；SIM 生产路径已内联至 `f203_encrypt_l18_l19` 尾部。
 * I/O：u[4,256] int32、v[256] int32 → c[1568] uint8（与 `golden/c.bin` 对拍）。
 * 设备例程见 `f203_tail_pack_ops.hpp`。
 */
#include "f203_encrypt_tail_layout.h"
#include "compute/f203_tail_pack_ops.hpp"
#include "kernel_operator.h"

using namespace AscendC;

/**
 * 仅 block0：逐 poly pack c₁(d=11)，再 pack c₂(d=5)。
 * @param uGm/vGm 时域输入；@param cGm 密文输出
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

    for (uint32_t p = 0; p < f203_tail::kPackK; ++p) {
        f203_tail::pack_one_u_poly_d11(gmU, gmC, p);
    }
    f203_tail::pack_v_poly_d5(gmV, gmC);
}

#ifndef __CCE_KT_TEST__
/** Host ACL 启动封装。 */
extern "C" void f203_encrypt_alg14_pack_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uGm,
                                           uint8_t *vGm, uint8_t *cGm)
{
    f203_encrypt_alg14_pack<<<blockDim, l2ctrl, stream>>>(uGm, vGm, cGm);
}
#endif
