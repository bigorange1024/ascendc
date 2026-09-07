/**
 * @file byteencode_l2_ub.hpp
 * @brief E10 L2：Compress 之后对整 poly 做真 ByteEncode_d（默认 d=4 → 128B）。
 *
 * 背景：D-exp-e10 — 在 E09 壳 Compress 后、SET(4) 前接入真 ByteEncode；非 TRACE stub。
 * 结论：仅 AIV0 在双 AIV Compress 完成后调用；算法自 `vendor/byteencode_d/`（拷自
 * pass-f203-byteencode-d-vec-k4），默认 BYTE_ENCODE_D_VEC=1。
 * 未采用：抄 Encrypt 整图；改原探针；假 ByteEncode TRACE。
 *
 * 输入：Compress_d 后 GM int32[256]（值 ∈ [0, 2^d-1]）。
 * 输出：GM uint8[128]（d=4 单 poly 编码比特流）。
 */
#ifndef TOY_E10_BYTEENCODE_L2_UB_HPP
#define TOY_E10_BYTEENCODE_L2_UB_HPP

#include "byte_encode_d_config.hpp"
#include "byte_encode_d_vec.hpp"
#include "kernel_operator.h"

namespace ByteEncodeL2Toy {

/**
 * 对整 poly 做真 ByteEncode_d：GM 压缩系数 → GM 打包字节流。
 * @param compGm    int32[256] Compress 输出（读）
 * @param encodedGm uint8[kOutBytes] ByteEncode 输出（写；d=4 时 128B）
 * 前置：调用方已 PipeBarrier；仅 AIV0 调用；comp 与 encoded 可不同 GM 区（本壳 encoded 覆写 out 前缀）。
 */
__aicore__ inline void EncodeFullPoly(GM_ADDR compGm, GM_ADDR encodedGm)
{
    constexpr uint32_t n = byte_encode_d::kPolyLen;
    constexpr uint32_t outBytes = byte_encode_d::kOutBytes;

    AscendC::GlobalTensor<int32_t> gmIn;
    AscendC::GlobalTensor<uint8_t> gmOut;
    gmIn.SetGlobalBuffer((__gm__ int32_t *)compGm, n);
    gmOut.SetGlobalBuffer((__gm__ uint8_t *)encodedGm, outBytes);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmpBuf;

    pipe.InitBuffer(inQ, 1, n * sizeof(int32_t));
    pipe.InitBuffer(outQ, 1, outBytes);
#if BYTE_ENCODE_D_VEC >= 2
    constexpr uint32_t scratchInt32 = 1024U;
#else
    const uint32_t scratchInt32 = n;
#endif
    pipe.InitBuffer(tmpBuf, scratchInt32 * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> inLoc = inQ.AllocTensor<int32_t>();
    AscendC::LocalTensor<uint8_t> outLoc = outQ.AllocTensor<uint8_t>();
    AscendC::LocalTensor<int32_t> tmpLoc = tmpBuf.Get<int32_t>();

    AscendC::DataCopy(inLoc, gmIn, n);
    AscendC::PipeBarrier<PIPE_ALL>();

    byte_encode_d::poly_byte_encode_local(outLoc, inLoc, tmpLoc);

    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmOut, outLoc, outBytes);

    inQ.FreeTensor(inLoc);
    outQ.FreeTensor(outLoc);
}

} // namespace ByteEncodeL2Toy

#endif
