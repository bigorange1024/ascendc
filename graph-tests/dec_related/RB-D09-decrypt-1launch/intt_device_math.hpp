/**
 * @file intt_device_math.hpp
 * @brief RB-D09：w←INTT(ŵ)；m←ByteEncode₁(Compress₁(v−w))（NPU 安全写出）。
 *
 * 契约：Alg.10 InverseNTT；Compress₁=round(x·2/q) mod 2；BE₁ LSB-first 打包 32B。
 * m 经 LocalTensor + DataCopy 写出；禁 uint8 GM SetValue。
 */
#ifndef RB_D09_INTT_DEVICE_MATH_HPP
#define RB_D09_INTT_DEVICE_MATH_HPP

#include "kernel_operator.h"
#include "tiling.h"
#include <cstdint>

namespace rb_d09 {

__aicore__ inline int32_t ModQIntt(int32_t x)
{
    int32_t r = x % d09::kQ;
    if (r < 0) {
        r += d09::kQ;
    }
    return r;
}

/** FIPS 203 Alg.10 InverseNTT（就地）。ζ^{−BitRev7(i)}≡−zetas[i]。 */
__aicore__ inline void InverseNTT(AscendC::LocalTensor<int32_t> &f,
                                  const AscendC::LocalTensor<int32_t> &zetas)
{
    int32_t zi = 127;
    for (int32_t length = 2; length <= 128; length *= 2) {
        for (int32_t start = 0; start < d09::kPolyN; start += 2 * length) {
            const int32_t zeta = ModQIntt(-zetas.GetValue(static_cast<uint32_t>(zi)));
            zi -= 1;
            for (int32_t j = start; j < start + length; ++j) {
                const int32_t t = f.GetValue(static_cast<uint32_t>(j));
                const int32_t u = f.GetValue(static_cast<uint32_t>(j + length));
                f.SetValue(static_cast<uint32_t>(j), ModQIntt(t + u));
                f.SetValue(static_cast<uint32_t>(j + length), ModQIntt(zeta * (t - u)));
            }
        }
    }
    for (int32_t i = 0; i < d09::kPolyN; ++i) {
        const int32_t x = f.GetValue(static_cast<uint32_t>(i));
        f.SetValue(static_cast<uint32_t>(i), ModQIntt(x * d09::kInttScale));
    }
}

/**
 * Compress₁：统一整数式 (C·u + 2^{35}) >> 36 & 1；C=41285357。
 * 背景：与 liboqs / host golden 同一常数路径；未采用学校式 (2x+q/2)/q（边界易偏）。
 * @param u 系数 ∈ [0,q)
 * @return 0 或 1
 */
__aicore__ inline int32_t Compress1(int32_t u)
{
    const int64_t s =
        d09::kCompressC * static_cast<int64_t>(u) + d09::kCompress1Bias;
    return static_cast<int32_t>(s >> d09::kCompress1Shift) & 1;
}

/**
 * AIV0：INTT(ŵ)→w；Compress₁(v−w) → BE₁ → DataCopy 到 OFF_M / mOut。
 * @param ws   统一 workspace
 * @param mOut Host 可见 m[32] 输出缓冲
 */
__aicore__ inline void ComputeInttAndExtract(GM_ADDR ws, GM_ADDR mOut)
{
    using namespace d09;

    AscendC::GlobalTensor<int32_t> gmWHat;
    AscendC::GlobalTensor<int32_t> gmV;
    AscendC::GlobalTensor<int32_t> gmZetas;
    AscendC::GlobalTensor<int32_t> gmW;
    AscendC::GlobalTensor<uint8_t> gmMWs;
    AscendC::GlobalTensor<uint8_t> gmMOut;
    gmWHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_W_HAT),
                           static_cast<uint32_t>(kPolyN));
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_V),
                        static_cast<uint32_t>(kPolyN));
    gmZetas.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_ZETAS),
                            static_cast<uint32_t>(kZetaN));
    gmW.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_W),
                        static_cast<uint32_t>(kPolyN));
    gmMWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_M),
                          static_cast<uint32_t>(kMBytes));
    gmMOut.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(mOut),
                           static_cast<uint32_t>(kMBytes));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> quePoly;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queV;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queZeta;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queM;
    pipe.InitBuffer(quePoly, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queV, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queZeta, 1, static_cast<uint32_t>(kZetasBytes));
    pipe.InitBuffer(queM, 1, static_cast<uint32_t>(kMBytes));

    AscendC::LocalTensor<int32_t> poly = quePoly.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> vLoc = queV.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> zetas = queZeta.AllocTensor<int32_t>();
    AscendC::LocalTensor<uint8_t> mLoc = queM.AllocTensor<uint8_t>();

    AscendC::DataCopy(zetas, gmZetas, static_cast<uint32_t>(kZetaN));
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(poly, gmWHat, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    InverseNTT(poly, zetas);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmW, poly, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    AscendC::DataCopy(vLoc, gmV, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    // m 先在 UB 组包，再 DataCopy 落 GM（NPU：禁 uint8 GlobalTensor::SetValue）
    for (int32_t bi = 0; bi < static_cast<int32_t>(kMBytes); ++bi) {
        uint8_t byte = 0;
        for (int32_t b = 0; b < 8; ++b) {
            const int32_t i = bi * 8 + b;
            const int32_t diff =
                ModQIntt(vLoc.GetValue(static_cast<uint32_t>(i)) -
                         poly.GetValue(static_cast<uint32_t>(i)));
            const int32_t bit = Compress1(diff);
            byte = static_cast<uint8_t>(byte | (static_cast<uint8_t>(bit) << b));
        }
        mLoc.SetValue(static_cast<uint32_t>(bi), byte);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmMWs, mLoc, static_cast<uint32_t>(kMBytes));
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmMOut, mLoc, static_cast<uint32_t>(kMBytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    quePoly.FreeTensor(poly);
    queV.FreeTensor(vLoc);
    queZeta.FreeTensor(zetas);
    queM.FreeTensor(mLoc);
}

} // namespace rb_d09

#endif
