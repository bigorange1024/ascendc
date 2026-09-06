/**
 * @file decompress_l2_ub.hpp
 * @brief E11 L2：INTT 之后对 half poly 做真 Decompress_1(μ) 并 mod-q 累加。
 *
 * 背景：D-exp-e11 — 在 E10 壳 INTT 后、Compress 前接入真 Decompress_1 消息嵌入。
 * 结论：双 AIV 各处理 128 系数；μ 由 Host 写入 ws[M1]（32B）；算法自 vendor/decompress_d/
 * decompress_d1_mu_embed.hpp（d=1 消息嵌入，≠ d=4 公式解压）。
 * 未采用：TRACE stub；抄 Encrypt 整图；改原探针。
 *
 * 输入：INTT 后 Barrett 归约的 half GM；μ GM 32B。
 * 输出：同址覆写 dst += Decompress_1(μ) (mod q)。
 */
#ifndef TOY_E11_DECOMPRESS_L2_UB_HPP
#define TOY_E11_DECOMPRESS_L2_UB_HPP

#include "basemul_half_ub.hpp"
#include "decompress_d1_mu_embed.hpp"
#include "kernel_operator.h"

namespace DecompressL2Toy {

/**
 * 对本核 half poly 做 Decompress_1(μ) 并 mod-q 累加到同一 GM。
 * @param halfDst     half 段起点（dst + subBlockID*(n/2)*4）
 * @param muGm        消息 μ 起点（ws+M1，32B）
 * @param coeffOffset 本 half 在整 poly 中的起始系数（0 或 128）
 * @param halfLen     系数个数（128）
 * 前置：调用方已 PipeBarrier；half 内系数已 canonical mod q。
 */
__aicore__ inline void DecompressMuAddHalfInPlace(GM_ADDR halfDst, GM_ADDR muGm, uint32_t coeffOffset,
                                                  uint32_t halfLen)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> muQ;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    AscendC::TBuf<AscendC::TPosition::VECCALC> muHalfBuf;

    pipe.InitBuffer(muQ, 1, F203_DECOMPRESS_D1_MSG_BYTES);
    pipe.InitBuffer(inQ, 1, halfLen * sizeof(int32_t));
    pipe.InitBuffer(outQ, 1, halfLen * sizeof(int32_t));
    pipe.InitBuffer(muHalfBuf, halfLen * sizeof(int32_t));

    AscendC::GlobalTensor<uint8_t> gmMu;
    AscendC::GlobalTensor<int32_t> gm;
    gmMu.SetGlobalBuffer((__gm__ uint8_t *)muGm, F203_DECOMPRESS_D1_MSG_BYTES);
    gm.SetGlobalBuffer((__gm__ int32_t *)halfDst, halfLen);

    AscendC::LocalTensor<uint8_t> mLocal = muQ.AllocTensor<uint8_t>();
    AscendC::DataCopy(mLocal, gmMu, F203_DECOMPRESS_D1_MSG_BYTES);
    AscendC::PipeBarrier<PIPE_ALL>();

    AscendC::LocalTensor<int32_t> muHalf = muHalfBuf.Get<int32_t>();
    decompress_d1::mu_embed_half_from_message_ub(mLocal, muHalf, coeffOffset, halfLen);
    muQ.FreeTensor(mLocal);

    AscendC::LocalTensor<int32_t> inLoc = inQ.AllocTensor<int32_t>();
    AscendC::DataCopy(inLoc, gm, halfLen);
    inQ.EnQue(inLoc);
    inLoc = inQ.DeQue<int32_t>();

    AscendC::LocalTensor<int32_t> outLoc = outQ.AllocTensor<int32_t>();
    for (uint32_t i = 0; i < halfLen; ++i) {
        const int32_t sum = inLoc.GetValue(static_cast<int32_t>(i)) + muHalf.GetValue(static_cast<int32_t>(i));
        outLoc.SetValue(static_cast<int32_t>(i), toy_e06_basemul::BarrettRed(sum));
    }

    AscendC::PipeBarrier<PIPE_ALL>();
    outQ.EnQue(outLoc);
    inQ.FreeTensor(inLoc);
    outLoc = outQ.DeQue<int32_t>();
    AscendC::DataCopy(gm, outLoc, halfLen);
    outQ.FreeTensor(outLoc);
}

} // namespace DecompressL2Toy

#endif
