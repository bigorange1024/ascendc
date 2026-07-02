/**
 * @file f203_kem_dec_g_entry.cpp
 * @brief Decaps K1 独立 AIV launch：G(m'‖h) → K' + coins（跨 launch 读 mGm，stream sync 后可见）。
 */
#include "f203_kem_dec_g.hpp"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void f203_kem_dec_g(GM_ADDR m_gm, GM_ADDR h_gm, GM_ADDR Kprime_gm, GM_ADDR coins_gm)
{
#if defined(ASCENDC_CPU_DEBUG)
    // CPU 孪生：AIV_ONLY（tikicpu 无 func_key/binary 概念，与 AIV_MODE 一致）
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
#else
    // SIM/NPU：MIX_AIC_1_2 占位——G(m'‖h) 的 SHA3-512 仍在 AIV 段执行，AIC 空跑。
    // 背景（2026-07-02 合并单库）：decrypt+encrypt 合库后 AIV-only=5（本核 + prep_a_hat/prep_re/
    //   g4_noise/at_r5）触 R1（单 binary AIV-only func_key≥5 → 507000，见 CAModel funckey 知识库）。
    //   本核是数据/哈希通路，按 P4 改 MIX 占位让出 1 个 AIV func_key，SIM AIV-only 回落到 4。
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    if (AscendC::GetSubBlockNum() == 1) {
        return;
    }
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
#endif

    F203KemDec::KemDecG(reinterpret_cast<__gm__ uint8_t *>(m_gm), reinterpret_cast<__gm__ uint8_t *>(h_gm),
                        reinterpret_cast<__gm__ uint8_t *>(Kprime_gm),
                        reinterpret_cast<__gm__ uint8_t *>(coins_gm));
}
