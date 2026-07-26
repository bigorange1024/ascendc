// @probe pass-fix-f203-alg19-kem-keygen-device-k3
// @file f203_keygen_prep_entry.cpp
// @layer host
// @role prep 设备 kernel 统一 entry（注册 f203_keygen_prep）。 / Prep device entry TU.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_kem.bin (1184B) + dk_kem.bin (2400B)；D13 PKE 中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_kem+dk_kem out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_a_hat16_config.h, f203_keygen_prep_ub.hpp
// @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。


/**
 * @file f203_keygen_prep_entry.cpp
 * @brief Alg.13 KeyGen 准备段设备内核入口（Launch 1，AIV_ONLY）。
 *
 * ## 流水线位置
 * 全链第二次之前：行 3–7 SampleNTT(Â) + 行 8–15 PRF/CBD 生成 ŝ/ê；
 * 输出 a_hat/src/ρ 留在 GM，供 `mmad_custom`（Launch 2）消费。
 *
 * ## 对齐
 * FIPS 203 Alg.13，ML-KEM-768（k=3）；与 golden 仅 I/O 等价（中间 GM 不落盘）。
 *
 * ## Launch
 * blockDim=F203_AHAT16_BLOCK_DIM（默认 2）：双 AIV 并行 Â 分片；block0 独占 PRF+CBD。
 */
#include "f203_a_hat16_config.h"
#include "f203_keygen_prep_ub.hpp"

/**
 * 设备内核：KeyGen prep 单 TPipe 编排入口。
 * @param seed_d_gm       输入 GM：uint32[1] 种子 d
 * @param a_hat_gm        输出 GM：Â[9,256] int32 行主序
 * @param prf_out_gm      中间 GM：PRF [8,128] uint8（CBD 输入）
 * @param src_gm          输出 GM：ŝ‖ê [6,256] int32
 * @param rho_gm          输出 GM：ρ[32] uint8（行 21 拼接用）
 * @param x_gm            保留签名（SHAKE 消息区）；本路径内嵌 ProcessInline，未用
 * @param lengths_gm      保留签名；未用
 * @param se_workspace_gm 保留签名；未用
 * @param se_tiling_gm    Host 填充的 ShakeGeneralTilingData
 * 前置：KERNEL_TYPE_AIV_ONLY；GetBlockIdx() < F203_AHAT16_BLOCK_DIM
 */
extern "C" __global__ __aicore__ void f203_keygen_prep(GM_ADDR seed_d_gm, GM_ADDR a_hat_gm, GM_ADDR prf_out_gm,
                                                       GM_ADDR src_gm, GM_ADDR rho_gm, GM_ADDR x_gm, GM_ADDR lengths_gm,
                                                       GM_ADDR se_workspace_gm, GM_ADDR se_tiling_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    // 保留参数满足历史入口签名；内嵌 SHAKE 不走外层 x/len/ws
    (void)x_gm;
    (void)lengths_gm;
    (void)se_workspace_gm;

    // 从 GM 读 32-bit 种子（Host 以 LE uint32 写入 seed_d.bin）
    const __gm__ uint32_t *seedPtr = reinterpret_cast<const __gm__ uint32_t *>(seed_d_gm);
    const uint32_t seed_d = seedPtr[0];

    F203KeygenPrep::BuildKeygenPrepSinglePipe(seed_d, AscendC::GetBlockIdx(),
                                              reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                                              reinterpret_cast<__gm__ uint8_t *>(prf_out_gm),
                                              reinterpret_cast<__gm__ int32_t *>(src_gm),
                                              reinterpret_cast<__gm__ uint8_t *>(rho_gm), se_tiling_gm);
}

#ifndef __CCE_KT_TEST__
/**
 * Host 侧 ACL 启动包装：<<<blockDim, l2ctrl, stream>>> 调 f203_keygen_prep。
 * @param blockDim 通常为 kPrepBlockDim（2）
 * 其余指针语义同设备内核参数。
 */
extern "C" void f203_keygen_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                    uint8_t *a_hat_gm, uint8_t *prf_out_gm, uint8_t *src_gm, uint8_t *rho_gm,
                                    uint8_t *x_gm, uint8_t *lengths_gm, uint8_t *se_workspace_gm,
                                    uint8_t *se_tiling_gm)
{
    f203_keygen_prep<<<blockDim, l2ctrl, stream>>>(seed_d_gm, a_hat_gm, prf_out_gm, src_gm, rho_gm, x_gm,
                                                   lengths_gm, se_workspace_gm, se_tiling_gm);
}
#endif
