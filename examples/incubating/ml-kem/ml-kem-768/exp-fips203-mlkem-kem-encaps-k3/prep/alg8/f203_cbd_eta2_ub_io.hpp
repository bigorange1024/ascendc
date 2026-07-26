// @probe pass-fix-f203-alg14-pke-encrypt-device-k3
// @file prep/alg8/f203_cbd_eta2_ub_io.hpp
// @layer prep
// @role prep/alg8：η=2 centered binomial（secret/noise）设备采样与 LUT；与 presample/alg7 链接成 prep 链。 / Alg.8 η=2 CBD prep helpers. 本文件 `f203_cbd_eta2_ub_io.hpp` 为该子模块组件。 / Component: f203_cbd_eta2_ub_io.hpp.
// @production_io D14 默认 I/O：input/ ek_pke.bin(1184B)+m.bin+coins.bin；output/c.bin(1088B)；中间 GM 不落盘。
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_cbd_eta2_sw_lut.hpp, kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。



// @encrypt_probe_note D14 prep 的 batch7 在 f203_encrypt_re_cbd.hpp 中逐行调用；本文件保留 polyvec6 批处理 helper。
/**
 * @file f203_cbd_eta2_ub_io.hpp
 * @brief P1b/P2：GM↔UB MTE DataCopy + PipeBarrier。
 *
 * 每行流水线（6 行或 P2 下每 AIV 3 行）：
 *   1. DataCopy 128B  prf_gm[row] → prfLocal
 *   2. PipeBarrier    — MTE 完成后再读 UB（无 barrier 时 SIM ~23k 但对拍 FAIL）
 *   3. SamplePolyCbd2RowSwLutUb
 *   4. PipeBarrier    — 计算完成后再写 GM
 *   5. DataCopy 256×int32 rowLocal → src_gm[row]
 *   6. PipeBarrier
 *
 * P1b：`F203_CBD_BLOCK_DIM==1`，block0 串行 6 行。
 * P2：`F203_CBD_BLOCK_DIM==2`，RowForBlock 分片（μ≈28872 tick）。
 *
 * Pipe 细同步（Opt-5 Phase 1+5）：CopyIn→PIPE_MTE2，Vector 后→PIPE_V；CopyOut 后 barrier 已删减（C-04，下一行 CopyIn 由 MTE2 覆盖）。
 * 本目录 E20 仅沿用已本地化后的同步结论；本轮 CPU/SIM 对拍覆盖该 CBD helper 的生产调用链。
 */
#pragma once

#include "f203_cbd_eta2_sw_lut.hpp"

#include "kernel_operator.h"

namespace F203CbdEta2 {

/** GM→UB 后等 MTE2；Vector 写 UB 后等 V。CopyOut 后无 barrier（Opt-5 C-04 删减，SIM 证毕）。 */
#define F203_CBD_SYNC_AFTER_COPYIN() AscendC::PipeBarrier<PIPE_MTE2>()
#define F203_CBD_SYNC_BEFORE_COPYOUT() AscendC::PipeBarrier<PIPE_V>()

#if !defined(F203_CBD_BLOCK_DIM)
#define F203_CBD_BLOCK_DIM 2
#endif

/** 复用外部 UB：scratch 首 128B 作 prf 行，rowQue 作 256×int32 行缓冲（KeyGen 单 TPipe 路径）。 */
__aicore__ inline void SamplePolyCbd2Batch8WithUb(uint32_t blockIdx, __gm__ const uint8_t *prf_gm,
                                                   __gm__ int32_t *src_gm,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &scratchBuf,
                                                   AscendC::TQue<AscendC::TPosition::VECOUT, 1> &rowQue)
{
#if F203_CBD_BLOCK_DIM == 1
    if (blockIdx != 0U) {
        return;
    }
#else
    if (blockIdx >= CBD_BLOCK_DIM) {
        return;
    }
#endif

    AscendC::GlobalTensor<uint8_t> prfGm;
    AscendC::GlobalTensor<int32_t> srcGm;
    prfGm.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prf_gm), PRF_TOTAL_BYTES);
    srcGm.SetGlobalBuffer(src_gm, SRC_COEFFS);

    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();

#if F203_CBD_BLOCK_DIM == 1
    for (uint32_t row = 0; row < ROWS; ++row) {
#else
    for (uint32_t i = 0; i < ROWS_PER_AIV; ++i) {
        const uint32_t row = RowForBlock(blockIdx, i);
#endif
        AscendC::DataCopy(prfLocal, prfGm[row * PRF_BYTES], PRF_BYTES);
        F203_CBD_SYNC_AFTER_COPYIN();

        AscendC::LocalTensor<int32_t> rowLocal = rowQue.AllocTensor<int32_t>();
        SamplePolyCbd2RowSwLutUb(rowLocal, prfLocal);
        F203_CBD_SYNC_BEFORE_COPYOUT();

        rowQue.EnQue(rowLocal);
        rowLocal = rowQue.DeQue<int32_t>();
        AscendC::DataCopy(srcGm[row * N], rowLocal, N);
        rowQue.FreeTensor(rowLocal);
    }
}

__aicore__ inline void SamplePolyCbd2Batch8DataCopy(uint32_t blockIdx, __gm__ const uint8_t *prf_gm,
                                                    __gm__ int32_t *src_gm)
{
#if F203_CBD_BLOCK_DIM == 1
    if (blockIdx != 0U) {
        return;
    }
#else
    if (blockIdx >= CBD_BLOCK_DIM) {
        return;
    }
#endif

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;
    pipe.InitBuffer(scratchBuf, PRF_BYTES);
    pipe.InitBuffer(rowQue, 1, static_cast<uint32_t>(N) * sizeof(int32_t));
    SamplePolyCbd2Batch8WithUb(blockIdx, prf_gm, src_gm, scratchBuf, rowQue);
}

}  // namespace F203CbdEta2
