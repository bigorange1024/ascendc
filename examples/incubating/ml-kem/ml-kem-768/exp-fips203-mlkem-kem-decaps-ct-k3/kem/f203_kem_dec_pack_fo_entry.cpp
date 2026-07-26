/**
 * @file f203_kem_dec_pack_fo_entry.cpp
 * @brief Phase-E 尾核（CPU）：Compress/ByteEncode 得到 c'，同核调用 KemDecFo 写最终 K。
 *
 * 流水线：u,v（Encrypt compute 输出）→ pack → c' → FO(c,c',z,K')→K。
 *
 * 平台分叉：
 *   - CPU：本核为末段；替代独立 `f203_encrypt_alg14_pack`。
 *   - SIM：FO 已并入探针本地 `f203_encrypt_l18_l19` 尾（T19i）；本 TU 仍编入，供 CPU twin。
 *
 * 未采用：独立 AIV `fo_only` 核（已删，减 launch）。
 * 对齐 customspec：设备 FO，禁 host 选支。
 */
#include "f203_tail_pack_ops.hpp"
#include "f203_encrypt_tail_layout.h"
#include "f203_kem_dec_fo.hpp"
#include "kernel_operator.h"

using namespace AscendC;

/**
 * CPU 末核：先 pack 再 FO。
 * @param uGm,vGm     时域 u（k 个 poly）与 v（1 poly），int32 GM
 * @param cPrimeGm    输出重加密密文 c'（1088B）
 * @param cInGm       输入密文 c（与 c' 比较）
 * @param zGm,KprimeGm,KoutGm  FO 三元：z、K'、最终 K
 * 前置：仅 blockIdx==0 执行；KERNEL_TYPE_AIV_ONLY。
 */
extern "C" __global__ __aicore__ void f203_kem_dec_pack_fo(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cPrimeGm, GM_ADDR cInGm,
                                                           GM_ADDR zGm, GM_ADDR KprimeGm, GM_ADDR KoutGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    // 单核完成 pack+FO；其它 block 直接退
    if (GetBlockIdx() != 0) {
        return;
    }

    GlobalTensor<int32_t> gmU;
    GlobalTensor<int32_t> gmV;
    GlobalTensor<uint8_t> gmC;
    gmU.SetGlobalBuffer((__gm__ int32_t *)uGm, f203_tail::kPackK * f203_tail::kPackN);
    gmV.SetGlobalBuffer((__gm__ int32_t *)vGm, f203_tail::kPackN);
    gmC.SetGlobalBuffer((__gm__ uint8_t *)cPrimeGm, F203_TAIL_C_BYTES);

    // 行 22–24 几何（ML-KEM-768）：先压 u 的 3 个 poly（d_u=10），再压 v（d_v=4）写入 c'
    for (uint32_t p = 0; p < f203_tail::kPackK; ++p) {
        f203_tail::pack_one_u_poly_d10(gmU, gmC, p);
    }
    f203_tail::pack_v_poly_d4(gmV, gmC);

    // 同核 FO：c≟c' → K' 或 J(z‖c)
    F203KemDec::KemDecFo(reinterpret_cast<__gm__ uint8_t *>(cInGm), reinterpret_cast<__gm__ uint8_t *>(cPrimeGm),
                         reinterpret_cast<__gm__ uint8_t *>(zGm), reinterpret_cast<__gm__ uint8_t *>(KprimeGm),
                         reinterpret_cast<__gm__ uint8_t *>(KoutGm));
}


#ifndef __CCE_KT_TEST__
/** Host 侧 launch 包装（非 KT 测试构建）。 */
extern "C" void f203_kem_dec_pack_fo_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uGm, uint8_t *vGm,
                                        uint8_t *cPrimeGm, uint8_t *cInGm, uint8_t *zGm, uint8_t *KprimeGm,
                                        uint8_t *KoutGm)
{
    f203_kem_dec_pack_fo<<<blockDim, l2ctrl, stream>>>(uGm, vGm, cPrimeGm, cInGm, zGm, KprimeGm, KoutGm);
}

#endif
