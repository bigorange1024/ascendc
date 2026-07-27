/**
 * @file f203_cbd_eta2_ub_io.hpp
 * @brief 默认路径：GM↔UB DataCopy + PipeBarrier + CBD_2 SWAR/LUT（ROWS=4）。
 *
 * 每行流水线：
 *   1. DataCopy 128B PRF 行：GM → UB
 *   2. PipeBarrier<PIPE_MTE2>：等待 CopyIn 完成
 *   3. load32 + SWAR + CBD2 LUT：UB → UB rowLocal[256] int32
 *   4. PipeBarrier<PIPE_V>：等待计算结果落地
 *   5. DataCopy 256×int32：UB → GM
 *
 * P2 默认 blockDim=2：AIV0 `{0,2}`，AIV1 `{1,3}`。CPU 孪生 launch=1 时，
 * 运行时 `GetBlockNum()==1`，block0 串行处理 4 行，保持 I/O 语义一致。
 */
#pragma once

#include "f203_cbd_eta2_sw_lut.hpp"

#include "kernel_operator.h"

namespace F203CbdEta2 {

/* CopyIn 后必须等待 MTE2，CopyOut 前必须等待 V。CPU 顺序执行会掩盖同步问题，
 * SIM/NPU 上这些 barrier 是正确性条件，不能当作调试开关。 */
#define F203_CBD2_SYNC_AFTER_COPYIN() AscendC::PipeBarrier<PIPE_MTE2>()
#define F203_CBD2_SYNC_BEFORE_COPYOUT() AscendC::PipeBarrier<PIPE_V>()

#if !defined(F203_CBD_BLOCK_DIM)
#define F203_CBD_BLOCK_DIM 2
#endif

/**
 * 单行 CBD_2 的完整 GM↔UB 流水线。
 * @param row      [in]  全局行号，范围 [0,4)
 * @param prfGm    [in]  GM 上的 prf_out 视图，长度 PRF_TOTAL_BYTES=512 uint8
 * @param srcGm    [out] GM 上的 src 视图，长度 SRC_COEFFS=1024 int32
 * @param prfLocal [in/out] VECCALC UB 暂存区，容量至少 128B
 * @param rowQue   [in/out] VECOUT 队列，容量为一行 256×int32
 */
__aicore__ inline void SamplePolyCbd2OneRowUb(uint32_t row, AscendC::GlobalTensor<uint8_t> &prfGm,
                                              AscendC::GlobalTensor<int32_t> &srcGm,
                                              AscendC::LocalTensor<uint8_t> &prfLocal,
                                              AscendC::TQue<AscendC::TPosition::VECOUT, 1> &rowQue)
{
    /* CopyIn：按行搬 128B，128 是 32B 的 4 倍，满足本探针连续 DataCopy 的块对齐。 */
    AscendC::DataCopy(prfLocal, prfGm[row * PRF_BYTES], PRF_BYTES);
    F203_CBD2_SYNC_AFTER_COPYIN();

    /* 计算：每 4 字节产 8 系数；rowQue 只保存当前一行，逐行复用 UB。 */
    AscendC::LocalTensor<int32_t> rowLocal = rowQue.AllocTensor<int32_t>();
    SamplePolyCbd2RowSwLutUb(rowLocal, prfLocal);
    F203_CBD2_SYNC_BEFORE_COPYOUT();

    /* CopyOut：写回 src[row, :]，输出 1024B，同样为 32B 对齐的整行。 */
    rowQue.EnQue(rowLocal);
    rowLocal = rowQue.DeQue<int32_t>();
    AscendC::DataCopy(srcGm[row * N], rowLocal, N);
    rowQue.FreeTensor(rowLocal);
}

/**
 * 批处理主体：按编译/运行 blockDim 决定串行 4 行或双 AIV 2+2 行。
 * @param blockIdx   [in] 当前 AIV block 编号
 * @param prf_gm     [in] GM 指针，形状 [4,128] uint8
 * @param src_gm     [out] GM 指针，形状 [4,256] int32
 * @param scratchBuf [in/out] PRF 行 UB 暂存区
 * @param rowQue     [in/out] 输出行队列
 */
__aicore__ inline void SamplePolyCbd2Batch4WithUb(uint32_t blockIdx, __gm__ const uint8_t *prf_gm,
                                                   __gm__ int32_t *src_gm,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &scratchBuf,
                                                   AscendC::TQue<AscendC::TPosition::VECOUT, 1> &rowQue)
{
#if F203_CBD_BLOCK_DIM == 1
    if (blockIdx != 0U) {
        return;
    }
#else
    if (blockIdx >= AscendC::GetBlockNum()) {
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
        SamplePolyCbd2OneRowUb(row, prfGm, srcGm, prfLocal, rowQue);
    }
#else
    if (AscendC::GetBlockNum() == 1U) {
        if (blockIdx != 0U) {
            return;
        }
        /* CPU 孪生固定单 block：不改变编译期默认路径，只在运行时串行 4 行。 */
        for (uint32_t row = 0; row < ROWS; ++row) {
            SamplePolyCbd2OneRowUb(row, prfGm, srcGm, prfLocal, rowQue);
        }
    } else {
        /* 双 AIV：按锁定行表各处理 2 行。 */
        for (uint32_t i = 0; i < ROWS_PER_AIV; ++i) {
            const uint32_t row = RowForBlock(blockIdx, i);
            SamplePolyCbd2OneRowUb(row, prfGm, srcGm, prfLocal, rowQue);
        }
    }
#endif
}

/**
 * 默认入口：初始化本核 UB 资源后调用 Batch4WithUb。
 * @param blockIdx 当前 AIV block 编号
 * @param prf_gm   GM 输入，prf_out[4,128] uint8
 * @param src_gm   GM 输出，src[4,256] int32
 */
__aicore__ inline void SamplePolyCbd2Batch4DataCopy(uint32_t blockIdx, __gm__ const uint8_t *prf_gm,
                                                    __gm__ int32_t *src_gm)
{
#if F203_CBD_BLOCK_DIM == 1
    if (blockIdx != 0U) {
        return;
    }
#else
    if (blockIdx >= AscendC::GetBlockNum()) {
        return;
    }
#endif

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;
    pipe.InitBuffer(scratchBuf, PRF_BYTES);
    pipe.InitBuffer(rowQue, 1, static_cast<uint32_t>(N) * sizeof(int32_t));
    SamplePolyCbd2Batch4WithUb(blockIdx, prf_gm, src_gm, scratchBuf, rowQue);
}

}  // namespace F203CbdEta2
