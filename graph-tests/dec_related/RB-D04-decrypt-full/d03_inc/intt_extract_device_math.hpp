/**
 * @file intt_extract_device_math.hpp
 * @brief RB-D03 设备侧：Alg.10 InverseNTT(ŵ)→w；Alg.15 尾段 extract → m[32]。
 *
 * 流水线位置：MIX AIV0 在握手 Wait(3) 后执行本文件。
 * 数学契约（FIPS 203）：
 *   - w ← InverseNTT(ŵ)（Alg.10；ζ^{−BitRev7}≡−zetas[i]；末乘 128^{−1}=3303）
 *   - m ← ByteEncode₁(Compress₁(v − w))（统一整数 Compress d=1）
 * 布局：ŵ/v 与 D02/D01 同为 int32[256] ND；本刀无 Tag5T pad（标量路径）。
 *
 * 实现形态：AIV 标量 + LocalTensor（UB）；非向量化/非 Cube Stage 性能路径。
 * 写出：m 经 UB+DataCopy（X12）；禁 GlobalTensor::SetValue 写业务 GM。
 *
 * **禁止** 抄 alg15 / T25 / decrypt / encaps / frozen 设备源码。
 */
#ifndef RB_D03_INTT_EXTRACT_DEVICE_MATH_HPP
#define RB_D03_INTT_EXTRACT_DEVICE_MATH_HPP

#include "kernel_operator.h"
#include "d03_inc/tiling.h"
#include <cstdint>

namespace rb_d03 {

/**
 * 规范到 [0, q)。
 * @param x 任意 int32（可负）
 */
__aicore__ inline int32_t ModQ(int32_t x)
{
    int32_t r = x % tiling::kQ;
    if (r < 0) {
        r += tiling::kQ;
    }
    return r;
}

/**
 * FIPS 203 Alg.10 InverseNTT（就地）。
 * 背景：表 kMlkemZetas[i]=ζ^{BitRev7(i)}；Alg.10 用 ζ^{−BitRev7(i)}≡−zetas[i] (mod q)
 * （因 ζ^{128}≡−1，与 T10 topology_math 标量路径自洽）。
 * @param f     LocalTensor[256] NTT 域 → 时域
 * @param zetas LocalTensor[128] Host 预喂
 */
__aicore__ inline void InverseNTT(AscendC::LocalTensor<int32_t> &f,
                                  const AscendC::LocalTensor<int32_t> &zetas)
{
    int32_t zi = 127;
    for (int32_t length = 2; length <= 128; length *= 2) {
        for (int32_t start = 0; start < tiling::kPolyN; start += 2 * length) {
            const int32_t zeta = ModQ(-zetas.GetValue(static_cast<uint32_t>(zi)));
            zi -= 1;
            for (int32_t j = start; j < start + length; ++j) {
                const int32_t t = f.GetValue(static_cast<uint32_t>(j));
                const int32_t u = f.GetValue(static_cast<uint32_t>(j + length));
                f.SetValue(static_cast<uint32_t>(j), ModQ(t + u));
                f.SetValue(static_cast<uint32_t>(j + length), ModQ(zeta * (t - u)));
            }
        }
    }
    // 末乘 128^{−1} mod q
    for (int32_t i = 0; i < tiling::kPolyN; ++i) {
        const int32_t x = f.GetValue(static_cast<uint32_t>(i));
        f.SetValue(static_cast<uint32_t>(i), ModQ(x * tiling::kInttScale));
    }
}

/**
 * Compress_1：统一整数舍入 round(u·2/q) mod 2。
 * 公式：(C·u + 2^{35}) >> 36 & 1；C=41285357（docs/notes 统一整数总结 §2）。
 * @param u 系数 ∈ [0,q)
 * @return 0 或 1
 */
__aicore__ inline int32_t Compress1(int32_t u)
{
    const int64_t s = tiling::kCompressC * static_cast<int64_t>(u) + tiling::kCompress1Bias;
    return static_cast<int32_t>(s >> tiling::kCompress1Shift) & 1;
}

/**
 * ByteEncode_1：256 个 1-bit 系数 → 32 字节（LSB first per byte）。
 * @param outBytes LocalTensor uint8[32]
 * @param bits     LocalTensor int32[256]，每元素 0/1
 */
__aicore__ inline void ByteEncode1(AscendC::LocalTensor<uint8_t> &outBytes,
                                   const AscendC::LocalTensor<int32_t> &bits)
{
    for (int32_t b = 0; b < 32; ++b) {
        uint8_t acc = 0;
        for (int32_t k = 0; k < 8; ++k) {
            const int32_t bit = bits.GetValue(static_cast<uint32_t>(b * 8 + k)) & 1;
            acc = static_cast<uint8_t>(acc | (static_cast<uint8_t>(bit) << k));
        }
        outBytes.SetValue(static_cast<uint32_t>(b), acc);
    }
}

/**
 * AIV0：w←INTT(ŵ)；m←ByteEncode₁(Compress₁(v−w))；写出 m / 可选 w。
 *
 * @param ws    共享 workspace（W_HAT / V / ZETAS / W）
 * @param mOut  独立 m 落盘缓冲（32B）
 */
__aicore__ inline void ComputeInttAndExtract(GM_ADDR ws, GM_ADDR mOut)
{
    using namespace tiling;

    AscendC::GlobalTensor<int32_t> gmWHat;
    AscendC::GlobalTensor<int32_t> gmV;
    AscendC::GlobalTensor<int32_t> gmZetas;
    AscendC::GlobalTensor<int32_t> gmW;
    AscendC::GlobalTensor<uint8_t> gmMOut;
    gmWHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_W_HAT));
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_V));
    gmZetas.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_ZETAS));
    gmW.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_W));
    gmMOut.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(mOut));

    AscendC::TPipe pipe;
    // VECIN：UB 驻留（标量 Get/Set）；对齐 D02 契约，勿用 VECCALC
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qPoly;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qV;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qBits;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qZeta;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qM;
    constexpr uint32_t kPolyB = static_cast<uint32_t>(kPolyBytes);
    constexpr uint32_t kZetaB = static_cast<uint32_t>(kZetasBytes);
    constexpr uint32_t kMB = 32u; // m[32]；恰 32B 对齐，可精确 DataCopy
    pipe.InitBuffer(qPoly, 1, kPolyB);
    pipe.InitBuffer(qV, 1, kPolyB);
    pipe.InitBuffer(qBits, 1, kPolyB);
    pipe.InitBuffer(qZeta, 1, kZetaB);
    pipe.InitBuffer(qM, 1, kMB);

    AscendC::LocalTensor<int32_t> zetas = qZeta.AllocTensor<int32_t>();
    AscendC::DataCopy(zetas, gmZetas, static_cast<uint32_t>(kZetaN));
    AscendC::PipeBarrier<PIPE_ALL>();

    AscendC::LocalTensor<int32_t> poly = qPoly.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> vLoc = qV.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> bits = qBits.AllocTensor<int32_t>();
    AscendC::LocalTensor<uint8_t> mLoc = qM.AllocTensor<uint8_t>();

    // ---------- ŵ → UB；就地 INTT → w ----------
    // 背景：D02 输出平面 int32[256]；禁与 NTT 同核（X15）。
    AscendC::DataCopy(poly, gmWHat, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    InverseNTT(poly, zetas);
    AscendC::PipeBarrier<PIPE_ALL>();
    // 中间 w → ws（X12 DataCopy；便于分段对拍）
    AscendC::DataCopy(gmW, poly, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---------- extract：Compress₁(v − w) → bits；ByteEncode₁ → m ----------
    // 结论：Alg.15 尾 m = ByteEncode₁(Compress₁(v−INTT(ŵ)))。
    AscendC::DataCopy(vLoc, gmV, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    for (int32_t i = 0; i < kPolyN; ++i) {
        const int32_t diff = ModQ(vLoc.GetValue(static_cast<uint32_t>(i)) -
                                  poly.GetValue(static_cast<uint32_t>(i)));
        bits.SetValue(static_cast<uint32_t>(i), Compress1(diff));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    ByteEncode1(mLoc, bits);
    AscendC::PipeBarrier<PIPE_ALL>();

    // m[32] → 独立 out（X12；禁 SetValue 写业务 GM）
    AscendC::DataCopy(gmMOut, mLoc, kMB);
    AscendC::PipeBarrier<PIPE_ALL>();

    qM.FreeTensor(mLoc);
    qBits.FreeTensor(bits);
    qV.FreeTensor(vLoc);
    qPoly.FreeTensor(poly);
    qZeta.FreeTensor(zetas);
}

} // namespace rb_d03

#endif
