/**
 * @file f203_cbd_eta3_entry.cpp
 * @brief FIPS 203 Alg.8 SamplePolyCBD η=3 — 4×poly 设备核入口（ML-KEM-512，k=2）。
 *
 * 流水线位置：本探针 `pass-fix-f203-alg8-cbd-eta3-k2` 的唯一设备核编译单元。
 * Host 侧 `main.cpp` 通过 CPU 孪生 `ICPU_RUN_KF` 或 SIM/NPU 的
 * `f203_cbd_eta3_batch4_do` 启动本核。核内只调用 `SamplePolyCbd3Batch4`，
 * 输入为 `prf_out[4,192]` uint8，输出为 `src[4,256]` int32。
 *
 * 与 golden 的关系：本核只负责 I/O 变换；验收由 `scripts/verify_result.py`
 * 对拍 `golden_se_sampling.sample_poly_cbd3` 产出的 `golden_src.bin`，标准是
 * 逐元素 I/O 等价，不要求设备代码逐行复刻 golden 源码。
 */
#include "f203_cbd_eta3.hpp"

/**
 * Alg.8 CBD η=3 设备核函数（polyvec4 / batch4）。
 * @param prf_gm GM 输入指针，形状 [ROWS=4, PRF_BYTES=192] uint8
 * @param src_gm GM 输出指针，形状 [ROWS=4, N=256] int32（行 0/1=s，2/3=e）
 *
 * 前置条件：AIV_ONLY；默认 SIM/NPU launch blockDim=2。CPU 孪生 launch=1 时
 * `SamplePolyCbd3Batch4DataCopy` 内部按 `GetBlockNum()==1` 串行处理全部 4 行。
 */
extern "C" __global__ __aicore__ void f203_cbd_eta3_batch4(GM_ADDR prf_gm, GM_ADDR src_gm)
{
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

    F203CbdEta3::SamplePolyCbd3Batch4(reinterpret_cast<__gm__ uint8_t *>(prf_gm),
                                      reinterpret_cast<__gm__ int32_t *>(src_gm));
}

#ifndef __CCE_KT_TEST__
/* SIM/NPU 核启动壳；CPU 孪生由 main.cpp 直接调用 `f203_cbd_eta3_batch4`。 */
extern "C" void f203_cbd_eta3_batch4_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *prf_gm,
                                        uint8_t *src_gm)
{
    f203_cbd_eta3_batch4<<<blockDim, l2ctrl, stream>>>(prf_gm, src_gm);
}
#endif
