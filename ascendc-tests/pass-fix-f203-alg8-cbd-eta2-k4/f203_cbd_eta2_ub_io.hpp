/**
 * @file f203_cbd_eta2_ub_io.hpp
 * @brief P1b/P2：GM↔UB MTE DataCopy + PipeBarrier。
 *
 * 每行流水线（8 行或 P2 下每 AIV 4 行）：
 *   1. DataCopy 128B  prf_gm[row] → prfLocal
 *   2. PipeBarrier    — MTE 完成后再读 UB（无 barrier 时 SIM ~23k 但对拍 FAIL）
 *   3. SamplePolyCbd2RowSwLutUb
 *   4. PipeBarrier    — 计算完成后再写 GM
 *   5. DataCopy 256×int32 rowLocal → src_gm[row]
 *   6. PipeBarrier
 *
 * P1b：`F203_CBD_BLOCK_DIM==1`，block0 串行 8 行。
 * P2：`F203_CBD_BLOCK_DIM==2`，RowForBlock 双 AIV 分片（SIM/NPU blockDim=2）。
 * P2 + CPU 孪生：`ICPU_RUN_KF` 固定 blockDim=1（910B 每 block 会误起 1 AIC+2 AIV），
 *   内核见 `GetBlockNum()==1` 时 block0 串行 8 行，语义与 P2 对拍一致。
 *
 * Pipe 细同步（Opt-5 Phase 1+5）：CopyIn→PIPE_MTE2，Vector 后→PIPE_V；CopyOut 后 barrier 已删减（C-04）。
 */
#pragma once

#include "f203_cbd_eta2_sw_lut.hpp"

#include "kernel_operator.h"

namespace F203CbdEta2 {

/* 同步点宏：CopyIn（DataCopy GM→UB）后插入 PIPE_MTE2 屏障，确保搬运完成后再进入
 * 向量计算；计算完成后插入 PIPE_V 屏障，确保计算结果落地后再 CopyOut（UB→GM）。
 * 注：无此屏障时 SIM tick 更低（~23k）但对拍会 FAIL，说明该同步点是正确性必需，
 * 不是可选的性能开关（见文件头注释与 docs/notes 性能优化总结 §Pipe 同步）。 */
#define F203_CBD_SYNC_AFTER_COPYIN() AscendC::PipeBarrier<PIPE_MTE2>()
#define F203_CBD_SYNC_BEFORE_COPYOUT() AscendC::PipeBarrier<PIPE_V>()

/* F203_CBD_BLOCK_DIM 未在编译期通过 CMake 定义时，默认 2（P2 双 AIV，探针默认最优路径）。 */
#if !defined(F203_CBD_BLOCK_DIM)
#define F203_CBD_BLOCK_DIM 2
#endif

/**
 * 单行 CBD 采样的完整 GM↔UB 流水线：CopyIn → 计算 → CopyOut。
 * @param row      [in]     本次处理的全局行号，范围 [0, ROWS)
 * @param prfGm    [in]     GM 上的 PRF 输出全局张量视图（uint8，长度 PRF_TOTAL_BYTES）
 * @param srcGm    [out]    GM 上的 CBD 结果全局张量视图（int32，长度 SRC_COEFFS）
 * @param prfLocal [in/out] UB 暂存区（复用，scratchBuf），本次调用写入该行 PRF 数据
 * @param rowQue   [in/out] VECOUT 队列，用于该行输出 int32[N] 的 Alloc/EnQue/DeQue/Free
 * 前置条件：prfLocal 容量 >= PRF_BYTES；rowQue 已按 N*sizeof(int32_t) 初始化。
 */
__aicore__ inline void SamplePolyCbd2OneRowUb(uint32_t row, AscendC::GlobalTensor<uint8_t> &prfGm,
                                              AscendC::GlobalTensor<int32_t> &srcGm,
                                              AscendC::LocalTensor<uint8_t> &prfLocal,
                                              AscendC::TQue<AscendC::TPosition::VECOUT, 1> &rowQue)
{
    /* Step1 CopyIn：把第 row 行的 128B PRF 输出从 GM 搬入 UB */
    AscendC::DataCopy(prfLocal, prfGm[row * PRF_BYTES], PRF_BYTES);
    F203_CBD_SYNC_AFTER_COPYIN();  // 等 MTE2 搬运完成，避免计算读到未写完的 UB 数据

    /* Step2 计算：从 rowQue 分配本行输出缓冲，SWAR+LUT 计算 256 个 CBD 系数 */
    AscendC::LocalTensor<int32_t> rowLocal = rowQue.AllocTensor<int32_t>();
    SamplePolyCbd2RowSwLutUb(rowLocal, prfLocal);
    F203_CBD_SYNC_BEFORE_COPYOUT();  // 等向量计算完成，避免 CopyOut 搬出未写完的结果

    /* Step3 CopyOut：EnQue/DeQue 走队列生产者-消费者语义后，把结果写回 GM 对应行，释放缓冲 */
    rowQue.EnQue(rowLocal);
    rowLocal = rowQue.DeQue<int32_t>();
    AscendC::DataCopy(srcGm[row * N], rowLocal, N);
    rowQue.FreeTensor(rowLocal);
}

/**
 * P1b/P2 批处理主体：按编译期 F203_CBD_BLOCK_DIM 与运行时 blockDim 决定单核串行 8 行
 * 还是双 AIV 各 4 行，逐行调用 SamplePolyCbd2OneRowUb。
 * @param blockIdx   [in]     当前核的 block 编号（AscendC::GetBlockIdx()）
 * @param prf_gm     [in]     GM 指针，PRF 输出，形状 [ROWS, PRF_BYTES] uint8
 * @param src_gm     [out]    GM 指针，CBD 结果，形状 [ROWS, N] int32
 * @param scratchBuf [in/out] VECCALC TBuf，复用为每行 PRF 的 UB 暂存区（容量 >= PRF_BYTES）
 * @param rowQue     [in/out] VECOUT TQue，复用为每行输出的 UB 缓冲队列
 * 前置条件：调用侧（Batch8DataCopy）已完成 pipe.InitBuffer 对 scratchBuf/rowQue 的初始化。
 */
__aicore__ inline void SamplePolyCbd2Batch8WithUb(uint32_t blockIdx, __gm__ const uint8_t *prf_gm,
                                                   __gm__ int32_t *src_gm,
                                                   AscendC::TBuf<AscendC::TPosition::VECCALC> &scratchBuf,
                                                   AscendC::TQue<AscendC::TPosition::VECOUT, 1> &rowQue)
{
    /* 编译期 F203_CBD_BLOCK_DIM==1（P1b-single）：仅 block0 执行，其余核直接退出 */
#if F203_CBD_BLOCK_DIM == 1
    if (blockIdx != 0U) {
        return;
    }
#else
    /* 编译期允许双 AIV（P2）：运行时 blockIdx 超出实际启动的核数时退出（防御性判断） */
    if (blockIdx >= AscendC::GetBlockNum()) {
        return;
    }
#endif

    /* 将裸 GM 指针包装为带越界校验语义的 GlobalTensor 视图，长度分别为
     * 全部 8 行 PRF 总字节数 / 全部 8 行输出系数总数 */
    AscendC::GlobalTensor<uint8_t> prfGm;
    AscendC::GlobalTensor<int32_t> srcGm;
    prfGm.SetGlobalBuffer(const_cast<__gm__ uint8_t *>(prf_gm), PRF_TOTAL_BYTES);
    srcGm.SetGlobalBuffer(src_gm, SRC_COEFFS);

    AscendC::LocalTensor<uint8_t> prfLocal = scratchBuf.Get<uint8_t>();

#if F203_CBD_BLOCK_DIM == 1
    /* P1b-single：本核（block0）串行处理全部 8 行 */
    for (uint32_t row = 0; row < ROWS; ++row) {
        SamplePolyCbd2OneRowUb(row, prfGm, srcGm, prfLocal, rowQue);
    }
#else
    if (AscendC::GetBlockNum() == 1U) {
        /* 运行时实际只起了 1 个核（如 CPU 孪生固定 blockDim=1 的场景）：
         * 退化为单核串行 8 行，语义与 P2 双核结果一致，避免 tikicpu 按 launch
         * blockDim fork 出的多进程与本函数编译期分片假设（P2）不匹配。 */
        if (blockIdx != 0U) {
            return;
        }
        for (uint32_t row = 0; row < ROWS; ++row) {
            SamplePolyCbd2OneRowUb(row, prfGm, srcGm, prfLocal, rowQue);
        }
    } else {
        /* P2 双 AIV：按 RowForBlock 查表得到本 AIV 负责的 4 个全局行号，逐行处理 */
        for (uint32_t i = 0; i < ROWS_PER_AIV; ++i) {
            const uint32_t row = RowForBlock(blockIdx, i);
            SamplePolyCbd2OneRowUb(row, prfGm, srcGm, prfLocal, rowQue);
        }
    }
#endif
}

/**
 * P1b/P2 入口：初始化本核所需 UB 资源（TPipe/TQue/TBuf）后调用 SamplePolyCbd2Batch8WithUb。
 * @param blockIdx [in]  当前核的 block 编号
 * @param prf_gm   [in]  GM 指针，PRF 输出，形状 [ROWS, PRF_BYTES] uint8
 * @param src_gm   [out] GM 指针，CBD 结果，形状 [ROWS, N] int32
 * 被 f203_cbd_eta2.hpp 中的默认 SamplePolyCbd2Batch8（P1b/P2 分支）调用。
 */
__aicore__ inline void SamplePolyCbd2Batch8DataCopy(uint32_t blockIdx, __gm__ const uint8_t *prf_gm,
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

    /* 单 TPipe 管理本核 UB 资源：scratchBuf 复用为每行 PRF 暂存区（容量 = 单行字节数
     * PRF_BYTES，逐行覆盖写）；rowQue 深度 1，缓冲大小 = 单行输出字节数 N*sizeof(int32_t)。 */
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> rowQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;
    pipe.InitBuffer(scratchBuf, PRF_BYTES);
    pipe.InitBuffer(rowQue, 1, static_cast<uint32_t>(N) * sizeof(int32_t));
    SamplePolyCbd2Batch8WithUb(blockIdx, prf_gm, src_gm, scratchBuf, rowQue);
}

}  // namespace F203CbdEta2
