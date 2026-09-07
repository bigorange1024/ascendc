#ifndef ENCRYPT_SKEL_AIV_FUNC_HPP
#define ENCRYPT_SKEL_AIV_FUNC_HPP

/**
 * @file aiv_func.hpp
 * @brief 骨架 toy 的 AIV 侧 stub：hash/prep、填左矩阵、inner、encode magic。
 *
 * 禁止真 SHAKE/Keccak；用 Duplicate / 常量填数代替。
 * 双 AIV 分片：仅 AIV0 写左矩阵与 magic（避免双写竞态）；AIV1 参与 CrossCore SET
 * （与 Encrypt 双 AIV 均 SET flag1 同构），并做轻量 stub_inner。
 */

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"

/**
 * stub_hash/prep：UB 填常量后写一小段 STUB GM（禁止真哈希大量计算）。
 * @param subBlockID AIV 编号；仅 AIV0 写 GM，AIV1 空转后 barrier
 */
__aicore__ inline void StubHashPrep(int32_t subBlockID, GM_ADDR ws)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    pipe.InitBuffer(outQ, 1, tiling::kStubVecElems * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> ub = outQ.AllocTensor<int32_t>();
    // 常量填充：模拟「哈希输出」占位，非 SHAKE
    AscendC::Duplicate(ub, static_cast<int32_t>(0x11), tiling::kStubVecElems);
    outQ.EnQue(ub);
    ub = outQ.DeQue<int32_t>();
    if (subBlockID == 0) {
        AscendC::GlobalTensor<int32_t> stubGm;
        stubGm.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::STUB), tiling::kStubVecElems);
        AscendC::DataCopy(stubGm, ub, tiling::kStubVecElems);
    }
    outQ.FreeTensor(ub);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * 填左矩阵 A[kRows,kDim] int8 到 ws+S0：A[i] = (i % 64)+tag（NTT/INTT 共用槽）。
 * 仅 AIV0 写；AIV1 等待 barrier。尺寸随 SKEL_HEAVY（32 或 64 列维）。
 * @param tag 区分多轮：写入时叠加常量，便于肉眼区分轮次（非算法语义）
 */
__aicore__ inline void FillLeftMatrixA(int32_t subBlockID, GM_ADDR ws, int8_t tag)
{
    if (subBlockID != 0) {
        AscendC::PipeBarrier<PIPE_ALL>();
        return;
    }
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    pipe.InitBuffer(outQ, 1, tiling::kABytes * sizeof(int8_t));
    AscendC::LocalTensor<int8_t> ub = outQ.AllocTensor<int8_t>();
    for (uint32_t i = 0; i < tiling::kABytes; ++i) {
        ub.SetValue(i, static_cast<int8_t>((static_cast<int32_t>(i) % 64) + tag));
    }
    outQ.EnQue(ub);
    ub = outQ.DeQue<int8_t>();
    AscendC::GlobalTensor<int8_t> aGm;
    aGm.SetGlobalBuffer((__gm__ int8_t *)(ws + tiling::S0), tiling::kABytes);
    AscendC::DataCopy(aGm, ub, tiling::kABytes);
    outQ.FreeTensor(ub);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * stub_inner：读 STUB → Adds(+1) → Muls(×2) → 写回 STUB（少量向量算，非真内积）。
 * 双 AIV 都做同一无害计算（写回仍仅 AIV0），避免 SyncAll。
 */
__aicore__ inline void StubInner(int32_t subBlockID, GM_ADDR ws)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> midQ;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    pipe.InitBuffer(inQ, 1, tiling::kStubVecElems * sizeof(int32_t));
    pipe.InitBuffer(midQ, 1, tiling::kStubVecElems * sizeof(int32_t));
    pipe.InitBuffer(outQ, 1, tiling::kStubVecElems * sizeof(int32_t));

    AscendC::GlobalTensor<int32_t> stubGm;
    stubGm.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::STUB), tiling::kStubVecElems);

    AscendC::LocalTensor<int32_t> inUb = inQ.AllocTensor<int32_t>();
    AscendC::DataCopy(inUb, stubGm, tiling::kStubVecElems);
    inQ.EnQue(inUb);
    inUb = inQ.DeQue<int32_t>();

    AscendC::LocalTensor<int32_t> midUb = midQ.AllocTensor<int32_t>();
    AscendC::Adds(midUb, inUb, static_cast<int32_t>(1), tiling::kStubVecElems);
    midQ.EnQue(midUb);
    inQ.FreeTensor(inUb);
    midUb = midQ.DeQue<int32_t>();

    AscendC::LocalTensor<int32_t> outUb = outQ.AllocTensor<int32_t>();
    AscendC::Muls(outUb, midUb, static_cast<int32_t>(2), tiling::kStubVecElems);
    outQ.EnQue(outUb);
    midQ.FreeTensor(midUb);
    outUb = outQ.DeQue<int32_t>();

    if (subBlockID == 0) {
        AscendC::DataCopy(stubGm, outUb, tiling::kStubVecElems);
    }
    outQ.FreeTensor(outUb);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * μ-stub（对齐 PrefixEmbedMu 形态，非真编解码）：仅 AIV0。
 * 小块 DataCopy GM→UB → Adds(+1) → DataCopy UB→GM，模仿「读 e₂/m → 折叠 → 写回」。
 * 背景：l18 skipNtt 下 AIV0 先 PrefixEmbedMu 再 SET(4)；本 stub 测该前缀是否拖住握手。
 * 结论：仅形态占位；未采用真 μ embed / ByteDecode。
 * @param subBlockID AIV 编号；AIV1 只 barrier
 * @param ws workspace（读写 STUB 槽）
 */
__aicore__ inline void StubPrefixEmbedMu(int32_t subBlockID, GM_ADDR ws)
{
    if (subBlockID != 0) {
        AscendC::PipeBarrier<PIPE_ALL>();
        return;
    }
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    // 32B 对齐：搬 8 个 int32（远小于真 256 系数，仅形态）
    constexpr uint32_t kMuStubElems = 8;
    pipe.InitBuffer(inQ, 1, kMuStubElems * sizeof(int32_t));
    pipe.InitBuffer(outQ, 1, kMuStubElems * sizeof(int32_t));

    AscendC::GlobalTensor<int32_t> stubGm;
    stubGm.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::STUB), tiling::kStubVecElems);

    AscendC::LocalTensor<int32_t> inUb = inQ.AllocTensor<int32_t>();
    AscendC::DataCopy(inUb, stubGm, kMuStubElems);
    inQ.EnQue(inUb);
    inUb = inQ.DeQue<int32_t>();

    AscendC::LocalTensor<int32_t> outUb = outQ.AllocTensor<int32_t>();
    // 非真 μ 折叠：Adds(+1) 占位，证明 GM↔UB 往返发生
    AscendC::Adds(outUb, inUb, static_cast<int32_t>(1), kMuStubElems);
    outQ.EnQue(outUb);
    inQ.FreeTensor(inUb);
    outUb = outQ.DeQue<int32_t>();

    AscendC::DataCopy(stubGm, outUb, kMuStubElems);
    outQ.FreeTensor(outUb);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * stub_encode：仅 AIV0 写固定 magic 到 out GM（verify 只查 magic/长度）。
 * @param out 输出 GM，长度 tiling::kOutBytes
 * @param markB8 out[8]：0xA5 基线 / 0x04 GATE / 0x14 设备μ / 0x15 Hostμ
 */
__aicore__ inline void StubEncodeMagic(int32_t subBlockID, GM_ADDR out, uint8_t markB8)
{
    if (subBlockID != 0) {
        AscendC::PipeBarrier<PIPE_ALL>();
        return;
    }
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    pipe.InitBuffer(outQ, 1, tiling::kOutBytes * sizeof(uint8_t));
    AscendC::LocalTensor<uint8_t> ub = outQ.AllocTensor<uint8_t>();
    // 前 8 字节 ASCII "SKELENC1"
    for (uint32_t i = 0; i < 8; ++i) {
        ub.SetValue(i, static_cast<uint8_t>(tiling::kMagicPrefix[i]));
    }
    ub.SetValue(8, markB8);
    for (uint32_t i = 9; i < tiling::kOutBytes; ++i) {
        ub.SetValue(i, tiling::kMagicFill);
    }
    outQ.EnQue(ub);
    ub = outQ.DeQue<uint8_t>();
    AscendC::GlobalTensor<uint8_t> outGm;
    outGm.SetGlobalBuffer((__gm__ uint8_t *)out, tiling::kOutBytes);
    AscendC::DataCopy(outGm, ub, tiling::kOutBytes);
    outQ.FreeTensor(ub);
    AscendC::PipeBarrier<PIPE_ALL>();
}

#endif
