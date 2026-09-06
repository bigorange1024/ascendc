/**
 * @file basemul_half_ub.hpp
 * @brief E06 自包含标量 MultiplyNTTs（Alg.11/12）半 poly 路径。
 *
 * 积木来源：pass-fix-f203-alg11-12-multiplyntts-k4 的 ALG11_IMPL=0 标量镜像
 * （alg11_gammas.h + BaseCaseMultiply / MultiplyNTTs）；拷入本目录，未改原探针。
 *
 * 作用：在 UB 上对 [pairStart, pairEnd) 对做真 basemul（非 TRACE stub）。
 * 输入/输出：LocalTensor f/g/h 各至少覆盖对应系数；dtype int32；q=3329。
 * 前置：AIV 已写完本半区 NTT 结果到 fLocal；gLocal 已从 GM 拷入。
 *
 * 背景：TASK-E06 — L2 真 NTT 之后接入真 basemul；双 AIV 各做 64 对。
 * 结论：标量路径足够短向量对拍且避免 Gather/ROM 复杂度。
 * 未采用：整核向量 B2 Gather 路径；抄 Encrypt；改原 multiplyntts 目录。
 */
#ifndef TOY_E06_BASEMUL_HALF_UB_HPP
#define TOY_E06_BASEMUL_HALF_UB_HPP

#include "kernel_operator.h"
#include "alg11_gammas.h"

namespace toy_e06_basemul {

constexpr int32_t kQ = 3329;

/**
 * Barrett 模约化到 [0,q)（设备标量；与 multiplyntts alg11_12_ref 同步）。
 * @param x 任意 int32 中间值（可负）
 * @return x mod 3329，canonical
 */
__aicore__ inline int32_t BarrettRed(int32_t x)
{
    const int32_t q = kQ;
    int32_t t = x + (q & (x >> 31));
    int32_t t1 = (int32_t)(((int64_t)t * 78) >> 18);
    x = t - t1 * q;
    int32_t t2 = (int32_t)(((int64_t)x * 5039) >> 24);
    x = x - t2 * q;
    x = x - (q & ~((x - q) >> 31));
    return x;
}

/**
 * FIPS 203 Alg.12 BaseCaseMultiply（设备标量）。
 * @param c0,c1 输出偶/奇系数
 * @param a0,a1,b0,b1,gamma 输入对与 γ
 */
__aicore__ inline void BaseCaseMultiply(int32_t *c0, int32_t *c1, int32_t a0, int32_t a1, int32_t b0,
                                        int32_t b1, int32_t gamma)
{
    int32_t a1b1 = BarrettRed(a1 * b1);
    *c0 = BarrettRed(a0 * b0 + a1b1 * gamma);
    *c1 = BarrettRed(a0 * b1 + a1 * b0);
}

/**
 * 在 UB 上对半区做 Alg.11 MultiplyNTTs（真 basemul）。
 * @param hLocal 输出 LocalTensor（与 f 同布局；写回 pair 对应 2i/2i+1）
 * @param fLocal 左 poly（本半区 NTT 结果）
 * @param gLocal 右 poly ĝ（Host 预填，NTT 域玩具系数）
 * @param pairStart 起始对数（AIV0=0，AIV1=64）
 * @param pairEnd   结束对数（不含；AIV0=64，AIV1=128）
 * @param localBase 本 LocalTensor 对应的全局 pair 起点（与 pairStart 相同；索引相对 0）
 *
 * 布局：LocalTensor[0..] 对应全局系数 [pairStart*2 ..)；长度至少 (pairEnd-pairStart)*2。
 */
__aicore__ inline void MultiplyNttsHalfUb(AscendC::LocalTensor<int32_t> &hLocal,
                                          const AscendC::LocalTensor<int32_t> &fLocal,
                                          const AscendC::LocalTensor<int32_t> &gLocal, int32_t pairStart,
                                          int32_t pairEnd)
{
    // i：全局第 i 对；li：相对本半区 LocalTensor 的对下标
    for (int32_t i = pairStart; i < pairEnd; ++i) {
        const int32_t li = i - pairStart;
        const int32_t a0 = fLocal.GetValue(li * 2);
        const int32_t a1 = fLocal.GetValue(li * 2 + 1);
        const int32_t b0 = gLocal.GetValue(li * 2);
        const int32_t b1 = gLocal.GetValue(li * 2 + 1);
        int32_t c0 = 0;
        int32_t c1 = 0;
        BaseCaseMultiply(&c0, &c1, a0, a1, b0, b1, kAlg11Gammas[i]);
        hLocal.SetValue(li * 2, c0);
        hLocal.SetValue(li * 2 + 1, c1);
    }
}

/**
 * GM 半区 MultiplyNTTs：fGm/gGm → hGm，各 halfLen=128 个 int32。
 * @param hGm,fGm,gGm 全局半区起点（已按 subBlock 偏移）
 * @param pairStart,pairEnd 全局对数区间
 * @param halfLen 系数个数（128）
 *
 * 分段：CopyIn → 真 basemul → CopyOut；PipeBarrier 保证 MTE/标量可见性。
 */
__aicore__ inline void MultiplyNttsHalfGm(GM_ADDR hGm, GM_ADDR fGm, GM_ADDR gGm, int32_t pairStart,
                                          int32_t pairEnd, uint32_t halfLen)
{
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inF;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inG;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> outH;
    AscendC::GlobalTensor<int32_t> gmF;
    AscendC::GlobalTensor<int32_t> gmG;
    AscendC::GlobalTensor<int32_t> gmH;

    gmF.SetGlobalBuffer((__gm__ int32_t *)fGm);
    gmG.SetGlobalBuffer((__gm__ int32_t *)gGm);
    gmH.SetGlobalBuffer((__gm__ int32_t *)hGm);

    pipe.InitBuffer(inF, 1, halfLen * sizeof(int32_t));
    pipe.InitBuffer(inG, 1, halfLen * sizeof(int32_t));
    pipe.InitBuffer(outH, 1, halfLen * sizeof(int32_t));

    // ---- CopyIn：半区 f̂ / ĝ ----
    AscendC::LocalTensor<int32_t> fLocal = inF.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> gLocal = inG.AllocTensor<int32_t>();
    AscendC::DataCopy(fLocal, gmF, halfLen);
    AscendC::DataCopy(gLocal, gmG, halfLen);
    inF.EnQue(fLocal);
    inG.EnQue(gLocal);

    fLocal = inF.DeQue<int32_t>();
    gLocal = inG.DeQue<int32_t>();
    AscendC::LocalTensor<int32_t> hLocal = outH.AllocTensor<int32_t>();

    AscendC::PipeBarrier<PIPE_ALL>();
    // ---- 真 basemul（Alg.11/12 标量）----
    MultiplyNttsHalfUb(hLocal, fLocal, gLocal, pairStart, pairEnd);
    AscendC::PipeBarrier<PIPE_ALL>();

    outH.EnQue(hLocal);
    inF.FreeTensor(fLocal);
    inG.FreeTensor(gLocal);

    // ---- CopyOut：ĥ 写回 GM 半区 ----
    hLocal = outH.DeQue<int32_t>();
    AscendC::DataCopy(gmH, hLocal, halfLen);
    outH.FreeTensor(hLocal);
}

} // namespace toy_e06_basemul

#endif
