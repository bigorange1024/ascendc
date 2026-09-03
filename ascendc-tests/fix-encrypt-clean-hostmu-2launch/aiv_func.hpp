#ifndef ENCRYPT_CLEAN_HOSTMU_2LAUNCH_AIV_FUNC_HPP
#define ENCRYPT_CLEAN_HOSTMU_2LAUNCH_AIV_FUNC_HPP

/**
 * @file aiv_func.hpp
 * @brief 干净 Encrypt P0 的 AIV stub：hash/prep、填左矩阵、inner、encode magic。
 *
 * **禁止** PrefixEmbedMu / StubPrefixEmbedMu：μ 折叠仅在 Host（结构默认）。
 * 禁止真 SHAKE；用 Duplicate / 常量填数代替。
 * 双 AIV：AIV0 写左矩阵与 magic；AIV1 参与 CrossCore SET（与 Encrypt 双 AIV 同构）。
 */

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"

/**
 * stub_hash/prep：UB 填常量后写一小段 STUB GM。
 * @param subBlockID AIV 编号；仅 AIV0 写 GM
 */
__aicore__ inline void StubHashPrep(int32_t subBlockID, GM_ADDR ws)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    pipe.InitBuffer(outQ, 1, tiling::kStubVecElems * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> ub = outQ.AllocTensor<int32_t>();
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
 * L2 skipNtt 入口短 stub（at_jp 占位）：极短 Duplicate，**无** μ 前缀。
 * 背景：J-empty-trace-aic-wait4 — SET(4) 前禁止重 PrefixEmbed；本路径结构上无该函数。
 * @param subBlockID AIV 编号
 */
__aicore__ inline void StubAtJpLight(int32_t subBlockID, GM_ADDR ws)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    // 32B 对齐：8 个 int32
    constexpr uint32_t kLightElems = 8;
    pipe.InitBuffer(outQ, 1, kLightElems * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> ub = outQ.AllocTensor<int32_t>();
    AscendC::Duplicate(ub, static_cast<int32_t>(0x22), kLightElems);
    outQ.EnQue(ub);
    ub = outQ.DeQue<int32_t>();
    if (subBlockID == 0) {
        AscendC::GlobalTensor<int32_t> stubGm;
        stubGm.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::STUB), tiling::kStubVecElems);
        AscendC::DataCopy(stubGm, ub, kLightElems);
    }
    outQ.FreeTensor(ub);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * 填左矩阵 A[kRows,kDim] int8 到 ws+S0。
 * @param tag 轮次偏移（区分 L1/L2）
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
 * stub_inner：读 STUB → Adds(+1) → Muls(×2) → 写回（非真内积）。
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
 * stub_encode：仅 AIV0 写固定 magic 到 out GM。
 * @param markB8 out[8]；干净树固定 kMagicCleanHostMu
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
