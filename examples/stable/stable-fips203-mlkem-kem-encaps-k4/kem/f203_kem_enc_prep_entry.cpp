/**
 * @file f203_kem_enc_prep_entry.cpp
 * @brief Alg.20/17 Encaps prep 入口：先 KEM 头（H/G → K‖r），再 Encrypt prep（Â + CBD）。
 *
 * 注册符号：`f203_kem_enc_prep`（替换纯 `f203_encrypt_prep`，launch 数仍与 Encrypt 一致）。
 * 对齐 customspec：SIM 第 1 launch / CPU 第 1 launch；其后 compute/pack 不变。
 *
 * 双 AIV 职责（SIM/NPU，`F203_AHAT16_BLOCK_DIM==2`）：
 *   - **仅 block0** 跑 `KemEncInitHead`（写出 K、r）
 *   - 随后 **各核** 跑 `BuildEncryptPrepSinglePipe`（Â 分片）；CBD 仍仅在 block0 的 SinglePipe 内
 *   - block1 与 head 时间重叠：head 约占 +94k tick，不影响 block1 Â 并行
 *
 * CPU（`ASCENDC_CPU_DEBUG` 且 blockDim=2）：tikicpu 单 block 串行两次 Â 分片；头只跑一次。
 *
 * @param ek_gm / m_gm / K_gm / r_gm 见 KemEncInitHead（r 即 Alg.14 随机性输入）
 * @param a_hat_gm / prf_out_gm / re_gm / tiling_gm 同 vendored Encrypt prep
 *        （re_gm 承载 y‖e1‖e2）
 */
#include "f203_a_hat16_config.h"
#include "f203_encrypt_prep_ub.hpp"
#include "f203_kem_enc_init.hpp"

/**
 * 设备侧 prep 核：KEM 头 + Encrypt prep。
 * KERNEL_TYPE_AIV_ONLY：无 AIC；与 Encrypt prep 同管道类型。
 */
extern "C" __global__ __aicore__ void f203_kem_enc_prep(GM_ADDR ek_gm, GM_ADDR m_gm, GM_ADDR K_gm, GM_ADDR r_gm,
                                                        GM_ADDR a_hat_gm, GM_ADDR prf_out_gm, GM_ADDR re_gm,
                                                        GM_ADDR tiling_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const __gm__ uint8_t *ekPtr = reinterpret_cast<const __gm__ uint8_t *>(ek_gm);
    // r：头写完后供 PRF(r)→CBD；CPU/SIM 均通过同一 GM 指针语义
    const __gm__ uint8_t *rPtr = reinterpret_cast<const __gm__ uint8_t *>(r_gm);

#if defined(ASCENDC_CPU_DEBUG) && (F203_AHAT16_BLOCK_DIM == 2)
    // CPU 孪生：调度侧可能暴露多 block，但 Â 分片须串行两次；头只执行一次。
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
    F203KemEnc::KemEncInitHead(reinterpret_cast<__gm__ uint8_t *>(ek_gm), reinterpret_cast<__gm__ uint8_t *>(m_gm),
                               reinterpret_cast<__gm__ uint8_t *>(K_gm), reinterpret_cast<__gm__ uint8_t *>(r_gm));
    // 分片 0 / 1：与 Encrypt CPU 路径同构
    F203EncryptPrep::BuildEncryptPrepSinglePipe(ekPtr, rPtr, 0U, reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                                                reinterpret_cast<__gm__ uint8_t *>(prf_out_gm),
                                                reinterpret_cast<__gm__ int32_t *>(re_gm), tiling_gm);
    F203EncryptPrep::BuildEncryptPrepSinglePipe(ekPtr, rPtr, 1U, reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                                                reinterpret_cast<__gm__ uint8_t *>(prf_out_gm),
                                                reinterpret_cast<__gm__ int32_t *>(re_gm), tiling_gm);
#else
    // SIM/NPU：超界 block 直接返回；合法 block 先（仅 0）写头，再并行 Â
    if (AscendC::GetBlockIdx() >= static_cast<uint32_t>(F203_AHAT16_BLOCK_DIM)) {
        return;
    }
    if (AscendC::GetBlockIdx() == 0U) {
        F203KemEnc::KemEncInitHead(reinterpret_cast<__gm__ uint8_t *>(ek_gm),
                                   reinterpret_cast<__gm__ uint8_t *>(m_gm),
                                   reinterpret_cast<__gm__ uint8_t *>(K_gm),
                                   reinterpret_cast<__gm__ uint8_t *>(r_gm));
    }
    F203EncryptPrep::BuildEncryptPrepSinglePipe(ekPtr, rPtr, AscendC::GetBlockIdx(),
                                                reinterpret_cast<__gm__ int32_t *>(a_hat_gm),
                                                reinterpret_cast<__gm__ uint8_t *>(prf_out_gm),
                                                reinterpret_cast<__gm__ int32_t *>(re_gm), tiling_gm);
#endif
}

#ifndef __CCE_KT_TEST__
/** Host 侧 launch 包装（非 CPU_DEBUG 链接路径）。 */
extern "C" void f203_kem_enc_prep_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ek_gm, uint8_t *m_gm,
                                     uint8_t *K_gm, uint8_t *r_gm, uint8_t *a_hat_gm, uint8_t *prf_out_gm,
                                     uint8_t *re_gm, uint8_t *tiling_gm)
{
    f203_kem_enc_prep<<<blockDim, l2ctrl, stream>>>(ek_gm, m_gm, K_gm, r_gm, a_hat_gm, prf_out_gm, re_gm, tiling_gm);
}
#endif
