/**
 * @file f203_encrypt_tail_entry.cpp
 * @brief Alg.14 行 20 + 22–24：μ_embed 输出 + Compress/ByteEncode → c（单 launch AIV）。
 *
 * Compress d=5/d=11 向量路径抄自 pass-f203-compress-d-vec-k4；
 * ByteEncode d=5/d=11 分组 pack 抄自 pass-f203-byteencode-d-vec-k4（见 f203_tail_compress_byteencode.hpp）。
 * 输入 m/u/v 由 host 外部生成；本核不把 μ 加给 v（行 21 由 compute 探针负责）。
 */
#include "f203_encrypt_tail_layout.h"
#include "compute/f203_mu_embed.hpp"
#include "compute/f203_tail_compress_byteencode.hpp"
#include "kernel_operator.h"

using namespace AscendC;

namespace {

constexpr uint32_t kN = F203_TAIL_N;
constexpr uint32_t kK = F203_TAIL_K;

__aicore__ inline void canonicalize_poly_q(AscendC::LocalTensor<int32_t> &poly)
{
    for (uint32_t i = 0; i < kN; ++i) {
        uint32_t u = static_cast<uint32_t>(poly.GetValue(static_cast<int32_t>(i)));
        if (u >= static_cast<uint32_t>(F203_TAIL_Q)) {
            u = static_cast<uint32_t>(F203_TAIL_Q) - 1U;
        }
        poly.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(u));
    }
}

} // namespace

extern "C" __global__ __aicore__ void f203_encrypt_alg14_tail(GM_ADDR mGm, GM_ADDR uGm, GM_ADDR vGm,
                                                                GM_ADDR muEmbedGm, GM_ADDR cGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() != 0) {
        return;
    }

    GlobalTensor<uint8_t> gmM;
    GlobalTensor<int32_t> gmU;
    GlobalTensor<int32_t> gmV;
    GlobalTensor<int32_t> gmMu;
    GlobalTensor<uint8_t> gmC;
    gmM.SetGlobalBuffer((__gm__ uint8_t *)mGm, F203_TAIL_MSG_BYTES);
    gmU.SetGlobalBuffer((__gm__ int32_t *)uGm, kK * kN);
    gmV.SetGlobalBuffer((__gm__ int32_t *)vGm, kN);
    gmMu.SetGlobalBuffer((__gm__ int32_t *)muEmbedGm, kN);
    gmC.SetGlobalBuffer((__gm__ uint8_t *)cGm, F203_TAIL_C_BYTES);

    TPipe pipe;
    TQue<TPosition::VECIN, 1> queM;
    TQue<TPosition::VECIN, 1> quePoly;
    TQue<TPosition::VECOUT, 1> queMu;
    TQue<TPosition::VECOUT, 1> queBytes;
    TBuf<TPosition::VECCALC> bufComp;
    TBuf<TPosition::VECCALC> bufTmp;
    TBuf<TPosition::VECCALC> fBuf;

    pipe.InitBuffer(queM, 1, 64U);
    pipe.InitBuffer(quePoly, 1, kN * sizeof(int32_t));
    pipe.InitBuffer(queMu, 1, kN * sizeof(int32_t));
    pipe.InitBuffer(queBytes, 1, F203_TAIL_C1_POLY_BYTES);
    pipe.InitBuffer(bufComp, kN * sizeof(int32_t));
    pipe.InitBuffer(bufTmp, kN * sizeof(int32_t));
    // d=11 cast_div 需 3×256 float（与 pass compress 探针同拓扑）
    pipe.InitBuffer(fBuf, kN * sizeof(float) * 3U);

    LocalTensor<uint8_t> mLocal = queM.AllocTensor<uint8_t>();
    LocalTensor<int32_t> muLocal = queMu.AllocTensor<int32_t>();
    DataCopy(mLocal, gmM, F203_TAIL_MSG_BYTES);
    PipeBarrier<PIPE_ALL>();
    f203_tail::mu_embed_from_message_ub(mLocal, muLocal);
    PipeBarrier<PIPE_ALL>();
    DataCopy(gmMu, muLocal, kN);
    queM.FreeTensor(mLocal);
    queMu.FreeTensor(muLocal);

    LocalTensor<int32_t> polyLocal = quePoly.AllocTensor<int32_t>();
    LocalTensor<int32_t> compLocal = bufComp.Get<int32_t>();
    LocalTensor<int32_t> tmpLocal = bufTmp.Get<int32_t>();
    LocalTensor<uint8_t> bytesLocal = queBytes.AllocTensor<uint8_t>();
    LocalTensor<float> fRaw = fBuf.GetWithOffset<float>(kN, 0U);
    LocalTensor<float> fTmp = fBuf.GetWithOffset<float>(kN, kN * sizeof(float));
    LocalTensor<float> fQuot = fBuf.GetWithOffset<float>(kN, kN * 2U * sizeof(float));

    // --- 行 22–23：u polyvec → c₁（Compress₁₁ 向量）---
    for (uint32_t p = 0; p < kK; ++p) {
        DataCopy(polyLocal, gmU[p * kN], kN);
        PipeBarrier<PIPE_ALL>();
        canonicalize_poly_q(polyLocal);
        f203_tail::poly_compress_d11_vec(compLocal, polyLocal, tmpLocal, fRaw, fTmp, fQuot);
        PipeBarrier<PIPE_ALL>();
        f203_tail::poly_byte_encode_d11_local(bytesLocal, compLocal);
        PipeBarrier<PIPE_ALL>();
        DataCopy(gmC[p * F203_TAIL_C1_POLY_BYTES], bytesLocal, F203_TAIL_C1_POLY_BYTES);
    }

    // --- 行 24：v → c₂（Compress₅ 向量）---
    DataCopy(polyLocal, gmV, kN);
    PipeBarrier<PIPE_ALL>();
    canonicalize_poly_q(polyLocal);
    f203_tail::poly_compress_d5_vec(compLocal, polyLocal, tmpLocal);
    PipeBarrier<PIPE_ALL>();
    f203_tail::poly_byte_encode_d5_local(bytesLocal, compLocal);
    PipeBarrier<PIPE_ALL>();
    DataCopy(gmC[F203_TAIL_C1_BYTES], bytesLocal, F203_TAIL_C2_BYTES);

    quePoly.FreeTensor(polyLocal);
    queBytes.FreeTensor(bytesLocal);
}

#ifndef __CCE_KT_TEST__
extern "C" void f203_encrypt_alg14_tail_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *mGm,
                                           uint8_t *uGm, uint8_t *vGm, uint8_t *muEmbedGm, uint8_t *cGm)
{
    f203_encrypt_alg14_tail<<<blockDim, l2ctrl, stream>>>(mGm, uGm, vGm, muEmbedGm, cGm);
}
#endif
