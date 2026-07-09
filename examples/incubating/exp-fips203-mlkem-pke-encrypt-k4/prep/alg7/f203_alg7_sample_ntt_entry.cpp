// @probe stable-fips203-mlkem-pke-keygen-k4
// @file prep/alg7/f203_alg7_sample_ntt_entry.cpp
// @layer prep
// @role prep/alg7：FIPS203 Algorithm 7 SampleNTT（CBD+rej 采样 ŝ/ê 等）；含 XOF、rej 标量/向量与 compact LUT ROM。 / Alg.7 SampleNTT rejection-sampling prep kernels. 本文件 `f203_alg7_sample_ntt_entry.cpp` 为该子模块组件。 / Component: f203_alg7_sample_ntt_entry.cpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_alg7_config.h, f203_alg7_d12_vec.hpp, f203_alg7_layout.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file f203_alg7_sample_ntt_entry.cpp
 * @brief FIPS 203 Alg.7 SampleNTT 设备侧核函数入口（单 poly、单 TPipe 全链）。
 *
 * 流水线位置：
 *   Host main.cpp 读入 SEED_D + (j,i) → 本文件 __global__ 核 → F203Alg7::BuildAlg7SampleNttFromSeedD
 *   （见 f203_alg7_d12_vec.hpp）完成 SEED_D→ρ→SHAKE128(672B)→解交织→d1/d2[224]→rej→â[256]。
 *
 * 与 golden 关系：
 *   - 输入：input/seed_d.bin（uint32 LE）、input/poly_ij.bin（byte j, byte i）
 *   - 输出：output/d1.bin、output/d2.bin、output/a_hat.bin；F203_ALG7_DUMP_XOF=1 时另写 output/xof.bin
 *   - scripts/gen_data.py 生成 golden；scripts/verify_result.py 对拍
 *
 * 运行约束：blockDim=1、AIV_ONLY、仅 blockIdx==0 执行；SHAKE 与 rej 均在 UB 内完成，不经 GM 中转。
 * 生产默认：F203_ALG7_REJ_IMPL=1（向量 Mins 剔除 + Gather 交错 + 标量 compact）。
 */
#include "f203_alg7_config.h"
#include "f203_alg7_d12_vec.hpp"
#include "f203_alg7_layout.h"

/**
 * Alg.7 SampleNTT 设备核：单 (j,i) poly → NTT 域系数 â[256]。
 *
 * @param seed_d_gm  GM 上 SEED_D，uint32 LE，长度 4B
 * @param poly_ij_gm GM 上 (j,i)，各 1B，长度 2B；对应 Kyber 矩阵下标
 * @param xof_gm     GM 调试缓冲，672B；F203_ALG7_DUMP_XOF=0 时内核侧可传 nullptr 且不落盘
 * @param d1_gm      GM 输出 d1[224] int32，对拍中间量
 * @param d2_gm      GM 输出 d2[224] int32，对拍中间量
 * @param a_hat_gm   GM 输出 â[256] int32，Alg.7 最终 NTT 域多项式
 *
 * 前置条件：GetBlockIdx()==0；各 GM 指针已按 layout.h 尺寸分配并对齐。
 */
extern "C" __global__ __aicore__ void f203_alg7_sample_ntt_d12(GM_ADDR seed_d_gm, GM_ADDR poly_ij_gm, GM_ADDR xof_gm,
                                                               GM_ADDR d1_gm, GM_ADDR d2_gm, GM_ADDR a_hat_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    // 单核 POC：blockDim=1，非 0 块直接返回，避免多核重复写 GM
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }

    // 从 GM 解包 Host 写入的标量输入（无批量维）
    const __gm__ uint32_t *seedPtr = reinterpret_cast<const __gm__ uint32_t *>(seed_d_gm);
    const uint32_t seed_d = seedPtr[0];

    const __gm__ uint8_t *ijPtr = reinterpret_cast<const __gm__ uint8_t *>(poly_ij_gm);
    const uint8_t poly_j = ijPtr[0];
    const uint8_t poly_i = ijPtr[1];

    // 全链编排委托给 d12_vec.hpp；xof_gm 仅在 DUMP_XOF 编译开关下有效
    F203Alg7::BuildAlg7SampleNttFromSeedD(
        seed_d, poly_j, poly_i,
#if F203_ALG7_DUMP_XOF
        reinterpret_cast<__gm__ uint8_t *>(xof_gm),
#else
        nullptr,
#endif
        reinterpret_cast<__gm__ int32_t *>(d1_gm), reinterpret_cast<__gm__ int32_t *>(d2_gm),
        reinterpret_cast<__gm__ int32_t *>(a_hat_gm));
}

#ifndef __CCE_KT_TEST__
/**
 * Host 侧 ACL 启动包装：<<<blockDim>>> 调用设备核（非 CPU 孪生路径）。
 * @param blockDim 须为 1（与探针几何一致）
 */
extern "C" void f203_alg7_sample_ntt_d12_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *seed_d_gm,
                                            uint8_t *poly_ij_gm, uint8_t *xof_gm, uint8_t *d1_gm, uint8_t *d2_gm,
                                            uint8_t *a_hat_gm)
{
    f203_alg7_sample_ntt_d12<<<blockDim, l2ctrl, stream>>>(seed_d_gm, poly_ij_gm, xof_gm, d1_gm, d2_gm, a_hat_gm);
}
#endif
