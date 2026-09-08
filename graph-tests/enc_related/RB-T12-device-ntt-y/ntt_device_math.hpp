/**
 * @file ntt_device_math.hpp
 * @brief RB-T12 设备侧：FIPS 203 Alg.9 正向 NTT（poly-batch，整 poly）。
 *
 * 流水线位置：MIX AIV0 在 NTT 握手 Wait(3) 后，对 y[4×256] 逐 poly 做 NTT → ŷ。
 * 数学契约：Alg.9 + kMlkemZetas（Appendix A）；与 Host golden `mlkem_ntt` 对拍。
 *
 * 实现形态：AIV 标量 + LocalTensor（UB）；非向量化/非 Cube Stage1–3 性能路径。
 * 契约约束（KB §A2）：
 *   - 每个 poly 完整 256 系数同核处理（禁 hi/lo limbsplit）
 *   - 本路径无 Gather；若后刀改走 S1–S3 仍禁 Gather
 *
 * **禁止** 抄 alg14 / encrypt / encaps / decaps / frozen 设备源码。
 */
#ifndef RB_T12_NTT_DEVICE_MATH_HPP
#define RB_T12_NTT_DEVICE_MATH_HPP

#include "kernel_operator.h"
#include "tiling.h"
#include <cstdint>

namespace rb_t12 {

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
 * FIPS 203 Alg.9 正向 NTT（就地）。
 * 背景：ζ^{BitRev7(i)} 取自 Host 预喂 zetas[128]；i 从 1 递增到 127。
 * @param f LocalTensor[256] 时域 → NTT 域
 * @param zetas LocalTensor[128]
 */
__aicore__ inline void ForwardNTT(AscendC::LocalTensor<int32_t> &f,
                                  const AscendC::LocalTensor<int32_t> &zetas)
{
    int32_t zi = 1;
    for (int32_t length = 128; length >= 2; length /= 2) {
        for (int32_t start = 0; start < tiling::kPolyN; start += 2 * length) {
            const int32_t zeta = zetas.GetValue(static_cast<uint32_t>(zi));
            zi += 1;
            for (int32_t j = start; j < start + length; ++j) {
                const int32_t t = ModQ(zeta * f.GetValue(static_cast<uint32_t>(j + length)));
                const int32_t uj = f.GetValue(static_cast<uint32_t>(j));
                f.SetValue(static_cast<uint32_t>(j + length), ModQ(uj - t));
                f.SetValue(static_cast<uint32_t>(j), ModQ(uj + t));
            }
        }
    }
}

/**
 * 从 GM 拷一整 poly 到 LocalTensor。
 * @param dst UB；srcGm 起点；polyIdx 在向量中的 poly 下标（0..kKem-1）
 */
__aicore__ inline void CopyPolyIn(AscendC::LocalTensor<int32_t> &dst,
                                  AscendC::GlobalTensor<int32_t> &srcGm, int32_t polyIdx)
{
    AscendC::DataCopy(dst, srcGm[static_cast<uint32_t>(polyIdx * tiling::kPolyN)],
                      static_cast<uint32_t>(tiling::kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** 整 poly LocalTensor → GM。 */
__aicore__ inline void CopyPolyOut(AscendC::GlobalTensor<int32_t> &dstGm, int32_t polyIdx,
                                   const AscendC::LocalTensor<int32_t> &src)
{
    AscendC::DataCopy(dstGm[static_cast<uint32_t>(polyIdx * tiling::kPolyN)], src,
                      static_cast<uint32_t>(tiling::kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * AIV0：对 y[kKem] 逐 poly 正向 NTT，写出 ws+OFF_Y_HAT 与独立 yHatOut。
 *
 * poly-batch：循环按 poly 索引；每次握完整 256 系数（含概念上的 hi+lo）；
 * 不按 limb 跨核切分；不调用 Gather。
 *
 * @param ws      共享 workspace（含 Y / ZETAS / Y_HAT）
 * @param yHatOut 独立落盘缓冲（与 OFF_Y_HAT 同内容，便于 Host 直读）
 */
__aicore__ inline void ComputeYHatNtt(GM_ADDR ws, GM_ADDR yHatOut)
{
    using namespace tiling;

    AscendC::GlobalTensor<int32_t> gmY;
    AscendC::GlobalTensor<int32_t> gmZetas;
    AscendC::GlobalTensor<int32_t> gmYHatWs;
    AscendC::GlobalTensor<int32_t> gmYHatOut;
    gmY.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_Y));
    gmZetas.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_ZETAS));
    gmYHatWs.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_Y_HAT));
    gmYHatOut.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(yHatOut));

    AscendC::TPipe pipe;
    // VECIN：UB 驻留（标量 Get/Set）；对齐 T10 INTT 路径，勿用 VECCALC
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qPoly;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> qZeta;
    constexpr uint32_t kPolyB = static_cast<uint32_t>(kPolyBytes);
    constexpr uint32_t kZetaB = static_cast<uint32_t>(kZetasBytes);
    pipe.InitBuffer(qPoly, 1, kPolyB);
    pipe.InitBuffer(qZeta, 1, kZetaB);

    AscendC::LocalTensor<int32_t> zetas = qZeta.AllocTensor<int32_t>();
    AscendC::DataCopy(zetas, gmZetas, static_cast<uint32_t>(kZetaN));
    AscendC::PipeBarrier<PIPE_ALL>();

    AscendC::LocalTensor<int32_t> poly = qPoly.AllocTensor<int32_t>();
    for (int32_t p = 0; p < kKem; ++p) {
        // 完整 poly：一次拷入 256 系数，就地 NTT，再写回（禁 limbsplit）
        CopyPolyIn(poly, gmY, p);
        ForwardNTT(poly, zetas);
        AscendC::PipeBarrier<PIPE_ALL>();
        CopyPolyOut(gmYHatWs, p, poly);
        CopyPolyOut(gmYHatOut, p, poly);
    }

    qPoly.FreeTensor(poly);
    qZeta.FreeTensor(zetas);
}

} // namespace rb_t12

#endif
