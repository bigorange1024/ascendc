/**
 * ByteEncode₁₂-only（k=4）：2×AIV 各编码 2 poly → ek/sk。
 */
#include "byte_encode12_only.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"
#if BYTE_ENCODE12_PREFETCH >= 1 && (defined(ASCENDC_CPU_DEBUG) || defined(__DAV_C220_VEC__) || defined(__DAV_C310_VEC__))
#include "byte_encode12_rom_tables.cpp"
#endif

extern "C" __global__ __aicore__ void byte_encode12_custom(GM_ADDR dst, GM_ADDR t_hat, GM_ADDR ek_out, GM_ADDR sk_out,
                                                           TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    if (AIC) {
        return;
    }

    const int32_t subCoreIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    AivByteEncode12Only pipe(subCoreIdx, static_cast<uint32_t>(tiling.tileLength));
    pipe.Init(dst, t_hat, ek_out, sk_out);
    pipe.Process();
    KYBER_PIPE_ALL();
}
