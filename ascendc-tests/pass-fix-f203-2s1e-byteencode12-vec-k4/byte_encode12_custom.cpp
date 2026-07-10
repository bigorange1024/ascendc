/**
 * @file byte_encode12_custom.cpp
 * @brief ByteEncode₁₂-only 设备入口 kernel（k=4）：2×AIV 各编码 2 poly → ek/sk。
 *
 * 流水线位置：AscendC 设备侧入口；host（main.cpp）经 ICPU_RUN_KF / ACLRT_LAUNCH_KERNEL 启动。
 * 作用：MIX 1AIC+2AIV 下仅 AIV 执行 AivByteEncode12Only；AIC 空返回。
 * 与 golden 关系：写出 ek_polyvec / sk_polyvec，由 verify_result.py 与 golden_* 对拍。
 */
#include "byte_encode12_only.hpp"
#include "kernel_operator.h"
#include "kyber_limb6.hpp"
#include "tiling.h"
#if BYTE_ENCODE12_PREFETCH >= 1 && (defined(ASCENDC_CPU_DEBUG) || defined(__DAV_C220_VEC__) || defined(__DAV_C310_VEC__))
#include "byte_encode12_rom_tables.cpp"
#endif

/**
 * 设备入口：preset ŝ‖ê（dst）与 t̂ → ByteEncode₁₂ 写 ek/sk。
 * @param dst     GM int32，[12,256] 布局见 tiling.h（ŝ 双份 + ê）
 * @param t_hat   GM int32，[4,256] t̂ polyvec
 * @param ek_out  GM uint8，4×384 B = ByteEncode₁₂(t̂)
 * @param sk_out  GM uint8，4×384 B = ByteEncode₁₂(ŝ)
 * @param tiling  运行时 TilingData（tileLength=n=256）
 * 前置条件：KERNEL_TYPE_MIX_AIC_1_2；仅 subBlock（AIV）执行编码，AIC 直接 return。
 */
extern "C" __global__ __aicore__ void byte_encode12_custom(GM_ADDR dst, GM_ADDR t_hat, GM_ADDR ek_out, GM_ADDR sk_out,
                                                           TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    // AIC 子核不参与向量编码，直接退出
    const bool AIC = AscendC::GetSubBlockNum() == 1;
    if (AIC) {
        return;
    }

    // AIV：按 subBlockIdx 划分 poly 批（0→p[0,2)，1→p[2,4)）
    const int32_t subCoreIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());
    AivByteEncode12Only pipe(subCoreIdx, static_cast<uint32_t>(tiling.tileLength));
    pipe.Init(dst, t_hat, ek_out, sk_out);
    pipe.Process();
    KYBER_PIPE_ALL();
}
