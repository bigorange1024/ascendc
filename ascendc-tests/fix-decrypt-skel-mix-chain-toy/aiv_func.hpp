#ifndef DECRYPT_SKEL_AIV_FUNC_HPP
#define DECRYPT_SKEL_AIV_FUNC_HPP

/**
 * @file aiv_func.hpp
 * @brief Decrypt 握手骨架 toy 的 AIV stub：prep、填左矩阵、dot、encode magic。
 *
 * 流水线位置：被 mmad_custom.cpp 在 SoftSync / Cube 段调用；对齐 fused 的
 * stub_prep / stub_dot 占位，**禁止**真 SHAKE / unpack / su_dot。
 * 双 AIV：仅 AIV0 写左矩阵与 magic；AIV1 参与 CrossCore SET（与生产同构）。
 */

#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"

/**
 * stub_prep：UB Duplicate 常量后写一小段 STUB GM（禁止真 unpack/ByteDecode）。
 * @param subBlockID AIV 编号；仅 AIV0 写 GM，AIV1 barrier
 * @param ws workspace（写 STUB 槽）
 */
__aicore__ inline void StubPrep(int32_t subBlockID, GM_ADDR ws)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    pipe.InitBuffer(outQ, 1, tiling::kStubVecElems * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> ub = outQ.AllocTensor<int32_t>();
    // 常量填充：模拟 prep 输出占位，非真 unpack
    AscendC::Duplicate(ub, static_cast<int32_t>(0x21), tiling::kStubVecElems);
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
 * 填左矩阵 A[kRows,kDim] int8 到 ws+S0：A[i] = (i % 64)+tag。
 * 仅 AIV0 写；AIV1 等待 barrier。固定 16×32。
 * @param tag 区分 NTT/INTT 轮次（非算法语义）
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
 * stub_dot：读 STUB → Adds(+1) → 写回（仅 AIV0 写 GM）；模拟 su_dot 占位，非真内积。
 * @param subBlockID AIV 编号
 * @param ws workspace（读写 STUB）
 */
__aicore__ inline void StubDot(int32_t subBlockID, GM_ADDR ws)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    pipe.InitBuffer(inQ, 1, tiling::kStubVecElems * sizeof(int32_t));
    pipe.InitBuffer(outQ, 1, tiling::kStubVecElems * sizeof(int32_t));

    AscendC::GlobalTensor<int32_t> stubGm;
    stubGm.SetGlobalBuffer((__gm__ int32_t *)(ws + tiling::STUB), tiling::kStubVecElems);

    AscendC::LocalTensor<int32_t> inUb = inQ.AllocTensor<int32_t>();
    AscendC::DataCopy(inUb, stubGm, tiling::kStubVecElems);
    inQ.EnQue(inUb);
    inUb = inQ.DeQue<int32_t>();

    AscendC::LocalTensor<int32_t> outUb = outQ.AllocTensor<int32_t>();
    // 非真 su_dot：Adds(+1) 占位
    AscendC::Adds(outUb, inUb, static_cast<int32_t>(1), tiling::kStubVecElems);
    outQ.EnQue(outUb);
    inQ.FreeTensor(inUb);
    outUb = outQ.DeQue<int32_t>();

    if (subBlockID == 0) {
        AscendC::DataCopy(stubGm, outUb, tiling::kStubVecElems);
    }
    outQ.FreeTensor(outUb);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * stub_encode：仅 AIV0 写固定 magic 到 out GM（verify 只查 magic/长度）。
 * @param out 输出 GM，长度 tiling::kOutBytes
 * @param markB8 out[8]：合法路径 0x04
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
    // 前 8 字节 ASCII "SKELDEC1"
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
