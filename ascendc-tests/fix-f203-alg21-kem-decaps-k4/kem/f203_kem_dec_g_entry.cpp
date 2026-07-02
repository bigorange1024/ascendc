/**
 * @file f203_kem_dec_g_entry.cpp
 * @brief Decaps K1 独立 AIV launch：G(m'‖h) → K' + coins（跨 launch 读 mGm，stream sync 后可见）。
 */
#include "f203_kem_dec_g.hpp"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void f203_kem_dec_g(GM_ADDR m_gm, GM_ADDR h_gm, GM_ADDR Kprime_gm, GM_ADDR coins_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }

    F203KemDec::KemDecG(reinterpret_cast<__gm__ uint8_t *>(m_gm), reinterpret_cast<__gm__ uint8_t *>(h_gm),
                        reinterpret_cast<__gm__ uint8_t *>(Kprime_gm),
                        reinterpret_cast<__gm__ uint8_t *>(coins_gm));
}
