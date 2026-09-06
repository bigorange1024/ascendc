/**
 * @file compress_l2_ub.hpp
 * @brief E09 L2：INTT 之后对半 poly 做真 Compress_d（默认 d=4）。
 *
 * 背景：D-exp-e09 — 在 E08 壳 INTT+SET(4) 前接入真 Compress；非 TRACE stub。
 * 结论：双 AIV 各压本核 half（128 int32）；算法自 `vendor/compress_d/`（拷自
 * pass-f203-compress-d-vec-k4），默认向量 Barrett d=4。
 * 未采用：抄 Encrypt 整图；改原探针；假 Compress TRACE。
 *
 * 输入：INTT 后已 Barrett 归约到 [0,q) 的 half GM。
 * 输出：同址覆写压缩域 int32 ∈ [0, 2^d-1]。
 */
#ifndef TOY_E09_COMPRESS_L2_UB_HPP
#define TOY_E09_COMPRESS_L2_UB_HPP

#include "compress_d_config.hpp"
#include "compress_d_vec.hpp"
#include "kernel_operator.h"

namespace CompressL2Toy {

/**
 * 对本核 half poly 做真 Compress_d，结果写回同一 GM。
 * @param halfGm  half 段起点（dst + subBlockID*(n/2)*4）
 * @param halfLen 系数个数（本壳固定 128）
 * 前置：调用方已 PipeBarrier；half 内系数已 canonical mod q。
 */
__aicore__ inline void CompressHalfInPlace(GM_ADDR halfGm, uint32_t halfLen)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQ;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outQ;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmpBuf;

    pipe.InitBuffer(inQ, 1, halfLen * sizeof(int32_t));
    pipe.InitBuffer(outQ, 1, halfLen * sizeof(int32_t));
    pipe.InitBuffer(tmpBuf, halfLen * sizeof(int32_t));

    AscendC::GlobalTensor<int32_t> gm;
    gm.SetGlobalBuffer((__gm__ int32_t *)halfGm, halfLen);

    AscendC::LocalTensor<int32_t> inLoc = inQ.AllocTensor<int32_t>();
    AscendC::DataCopy(inLoc, gm, halfLen);
    inQ.EnQue(inLoc);
    inLoc = inQ.DeQue<int32_t>();

    AscendC::LocalTensor<int32_t> outLoc = outQ.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> tmpLoc = tmpBuf.Get<int32_t>();

#if !F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1
    // d=4/5：与 vendor poly_compress_barrett_vec 同公式，但 count=halfLen（非整 poly 256）
    {
        using AscendC::Adds;
        using AscendC::Muls;
        using AscendC::ShiftRight;
        const int32_t n = static_cast<int32_t>(halfLen);
        Muls(outLoc, inLoc, static_cast<int32_t>(F203_COMPRESS_BARRETT_MUL), n);
        Adds(outLoc, outLoc, static_cast<int32_t>(F203_COMPRESS_BARRETT_BIAS), n);
        ShiftRight(outLoc, outLoc, F203_COMPRESS_BARRETT_SHIFT, n);
        compress_d::mask_low_bits_i32(outLoc, tmpLoc, F203_COMPRESS_D_BITS, halfLen);
    }
#else
    // 标量 fallback（或 cast_div 档）：逐 lane 调 vendor scalar_compress_u32
    (void)tmpLoc;
    for (uint32_t i = 0; i < halfLen; ++i) {
        const uint32_t u = static_cast<uint32_t>(inLoc.GetValue(static_cast<int32_t>(i)));
        outLoc.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(compress_d::scalar_compress_u32(u)));
    }
#endif

    AscendC::PipeBarrier<PIPE_ALL>();
    outQ.EnQue(outLoc);
    inQ.FreeTensor(inLoc);
    outLoc = outQ.DeQue<int32_t>();
    AscendC::DataCopy(gm, outLoc, halfLen);
    outQ.FreeTensor(outLoc);
}

} // namespace CompressL2Toy

#endif
