/**
 * @file f203_kem_kg_finish_entry.cpp
 * @brief Launch-3 设备入口：FIPS 203 Alg.19 KeyGen_internal 尾段。
 *
 * 注册符号 f203_kem_kg_finish；由 main_kem_keygen 在 vendor prep+mmad 之后第 3 次 launch。
 * 核内仅调用 KemKgFinishImpl（H(ek)+UB 内 z+拼接 dk_kem）；不含 PKE 计算。
 *
 * @param seed_d_gm 见 KemKgFinishImpl
 * @param ek_pke_gm / dk_pke_gm vendor 输出
 * @param ek_kem_gm / dk_kem_gm KEM 输出 GM
 */
#include "f203_kem_kg_finish.hpp"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void f203_kem_kg_finish(GM_ADDR seed_d_gm, GM_ADDR ek_pke_gm, GM_ADDR dk_pke_gm,
                                                         GM_ADDR ek_kem_gm, GM_ADDR dk_kem_gm)
{
#if defined(ASCENDC_CPU_DEBUG)
    // CPU 孪生：纯 AIV，仅 block0 执行
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (AscendC::GetBlockIdx() != 0) {
        return;
    }
#else
    // SIM/NPU：MIX 占位释放 AIV-only func_key 名额（对齐 Decrypt prep 惯例）；
    // 实际哈希/拼接仍在 AIV 段，AIC（SubBlockNum==1）直接 return。
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
