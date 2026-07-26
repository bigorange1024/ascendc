/**
 * @file f203_cbd_eta2_entry.cpp
 * @brief FIPS 203 Alg.8 SamplePolyCBD η=2 — 6×poly 设备核入口（ML-KEM-768，k=3，η=2）。
 *
 * 流水线位置：本探针（`pass-fix-f203-alg8-cbd-eta2-k3`）的唯一设备核编译单元，
 * 被 `main.cpp` 通过 `ICPU_RUN_KF`（CPU 孪生）或 `f203_cbd_eta2_batch6_do`
 * （SIM/NPU）拉起。核内直接调用 `f203_cbd_eta2.hpp::SamplePolyCbd2Batch6`
 * 完成 Alg.8 采样计算，输入输出均为 GM 上的裸缓冲区（无 tiling 结构体参数，
 * 尺寸由编译期常量 F203CbdEta2::PRF_TOTAL_BYTES / SRC_COEFFS 固定）。
 * 与 golden 的关系：本核只负责计算，正确性由 Host 侧 `scripts/verify_result.py`
 * 与 `library/shared/fips203_se_sample/golden_se_sampling.py::sample_poly_cbd2`
 * 对拍验证（I/O 等价，非算法实现同构）。
 *
 * AIV_ONLY；P2 默认 blockDim=2（SIM/NPU）。CPU 孪生由 main 固定 blockDim=1 launch，
 * 内核见 GetBlockNum()==1 时 block0 串行 6 行（避免 tikicpu 按 block 误起多颗 AIC）。
 */
#include "f203_cbd_eta2.hpp"

/**
 * Alg.8 CBD η=2 设备核函数（polyvec6 / batch6）。
 * @param prf_gm GM 指针，输入 PRF 输出，形状 [ROWS=6, PRF_BYTES=128] uint8
 * @param src_gm GM 指针，输出 CBD 采样结果，形状 [ROWS=6, N=256] int32
 *               （行 0–2=ŝ 各分量，行 3–5=ê 各分量）
 * 前置条件：编译期 F203_CBD_BLOCK_DIM 决定分支——
 *   =1（P1b-single）：仅 blockIdx==0 执行，串行 6 行；
 *   其他（P2 默认）：blockIdx 超出实际启动核数（GetBlockNum）时直接返回，
 *     否则按 blockIdx 走双 AIV 分片（见 SamplePolyCbd2Batch6 内部实现）。
 */
extern "C" __global__ __aicore__ void f203_cbd_eta2_batch6(GM_ADDR prf_gm, GM_ADDR src_gm)
{
    /* 声明本核仅使用 AIV（向量核），不涉及 AIC（Cube）资源 */
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

#if F203_CBD_BLOCK_DIM == 1
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }
#else
    if (AscendC::GetBlockIdx() >= AscendC::GetBlockNum()) {
        return;
    }
#endif

    F203CbdEta2::SamplePolyCbd2Batch6(reinterpret_cast<__gm__ uint8_t *>(prf_gm),
                                      reinterpret_cast<__gm__ int32_t *>(src_gm));
}

#ifndef __CCE_KT_TEST__
/* SIM/NPU 路径的核启动壳；CPU 孪生由 main.cpp 走 ICPU_RUN_KF 直接调用核函数，不经过此壳。 */
extern "C" void f203_cbd_eta2_batch6_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *prf_gm,
                                        uint8_t *src_gm)
{
    f203_cbd_eta2_batch6<<<blockDim, l2ctrl, stream>>>(prf_gm, src_gm);
}
#endif
