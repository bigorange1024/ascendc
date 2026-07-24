/**
 * @file f203_encrypt_g4_noise_entry.cpp
 * @brief G4 Launch：INTT 后时域 u[4,256] + e₁；tr[256] + e₂ + μ → v[256]（mod q）。
 */
#include "f203_encrypt_g4_ub_scalar.hpp"
#include "f203_encrypt_layout.h"
#include "kernel_operator.h"

using namespace AscendC;

namespace {

constexpr int32_t kN = F203_ENCRYPT_N;
constexpr int32_t kK = F203_ENCRYPT_K;
constexpr int32_t kQ = F203_ENCRYPT_Q;
constexpr int32_t kHalfQ = (F203_ENCRYPT_Q + 1) / 2;

__aicore__ inline int32_t mod_q_add(int32_t a, int32_t b)
{
    int32_t x = a + b;
    x -= kQ * (x >= kQ);
    x += kQ * (x < 0);
    return x;
}

class KernelG4Noise {
public:
    __aicore__ inline void Init(GM_ADDR uGm, GM_ADDR e1Gm, GM_ADDR trGm, GM_ADDR e2Gm, GM_ADDR mGm, GM_ADDR vGm)
    {
        uGm_ = uGm;
        e1Gm_ = e1Gm;
        trGm_ = trGm;
        e2Gm_ = e2Gm;
        mGm_ = mGm;
        vGm_ = vGm;
        uG_.SetGlobalBuffer((__gm__ int32_t *)uGm, kK * kN);
        e1G_.SetGlobalBuffer((__gm__ int32_t *)e1Gm, kK * kN);
        trG_.SetGlobalBuffer((__gm__ int32_t *)trGm, kN);
        e2G_.SetGlobalBuffer((__gm__ int32_t *)e2Gm, kN);
        mG_.SetGlobalBuffer((__gm__ uint8_t *)mGm, F203_MSG_BYTES);
        vG_.SetGlobalBuffer((__gm__ int32_t *)vGm, kN);

#if !defined(ASCENDC_CPU_DEBUG)
        pipe_.InitBuffer(uQue_, 1, static_cast<uint32_t>(kN * sizeof(int32_t)));
        pipe_.InitBuffer(eQue_, 1, static_cast<uint32_t>(kN * sizeof(int32_t)));
        pipe_.InitBuffer(trQue_, 1, static_cast<uint32_t>(kN * sizeof(int32_t)));
        pipe_.InitBuffer(vQue_, 1, static_cast<uint32_t>(kN * sizeof(int32_t)));
        pipe_.InitBuffer(mQue_, 1, F203_MSG_BYTES);
#endif
    }

    __aicore__ inline void Process()
    {
#if defined(ASCENDC_CPU_DEBUG)
        f203_g4::add_noise_embed_scalar(uGm_, e1Gm_, trGm_, e2Gm_, mGm_, vGm_);
#else
        for (int32_t p = 0; p < kK; ++p) {
            LocalTensor<int32_t> uLoc = uQue_.AllocTensor<int32_t>();
            LocalTensor<int32_t> eLoc = eQue_.AllocTensor<int32_t>();
            const uint32_t off = static_cast<uint32_t>(p) * static_cast<uint32_t>(kN);
            DataCopy(uLoc, uG_[off], kN);
            DataCopy(eLoc, e1G_[off], kN);
            PipeBarrier<PIPE_ALL>();
            Add(uLoc, uLoc, eLoc, kN);
            PipeBarrier<PIPE_ALL>();
            for (int32_t c = 0; c < kN; ++c) {
                int32_t x = uLoc.GetValue(c);
                x -= kQ * (x >= kQ);
                x += kQ * (x < 0);
                uLoc.SetValue(c, x);
            }
            DataCopy(uG_[off], uLoc, kN);
            uQue_.FreeTensor(uLoc);
            eQue_.FreeTensor(eLoc);
            PipeBarrier<PIPE_MTE3>();
        }

        LocalTensor<int32_t> trLoc = trQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> e2Loc = eQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> vLoc = vQue_.AllocTensor<int32_t>();
        LocalTensor<uint8_t> mLoc = mQue_.AllocTensor<uint8_t>();
        DataCopy(trLoc, trG_[0], kN);
        DataCopy(e2Loc, e2G_[0], kN);
        DataCopy(mLoc, mG_[0], F203_MSG_BYTES);
        PipeBarrier<PIPE_ALL>();
        Add(vLoc, trLoc, e2Loc, kN);
        PipeBarrier<PIPE_ALL>();
        for (int32_t c = 0; c < kN; ++c) {
            const int32_t i = c / 8;
            const int32_t j = c % 8;
            int32_t x = vLoc.GetValue(c);
            if (i < 32) {
                const int32_t bit = (static_cast<int32_t>(mLoc.GetValue(i)) >> j) & 1;
                x = mod_q_add(x, kHalfQ * bit);
            }
            x -= kQ * (x >= kQ);
            x += kQ * (x < 0);
            vLoc.SetValue(c, x);
        }
        DataCopy(vG_[0], vLoc, kN);
        trQue_.FreeTensor(trLoc);
        eQue_.FreeTensor(e2Loc);
        vQue_.FreeTensor(vLoc);
        mQue_.FreeTensor(mLoc);
#endif
    }

private:
    GM_ADDR uGm_{nullptr};
    GM_ADDR e1Gm_{nullptr};
    GM_ADDR trGm_{nullptr};
    GM_ADDR e2Gm_{nullptr};
    GM_ADDR mGm_{nullptr};
    GM_ADDR vGm_{nullptr};
    TPipe pipe_;
    TQue<QuePosition::VECIN, 1> uQue_;
    TQue<QuePosition::VECIN, 1> eQue_;
    TQue<QuePosition::VECIN, 1> trQue_;
    TQue<QuePosition::VECIN, 1> vQue_;
    TQue<QuePosition::VECIN, 1> mQue_;
    GlobalTensor<int32_t> uG_;
    GlobalTensor<int32_t> e1G_;
    GlobalTensor<int32_t> trG_;
    GlobalTensor<int32_t> e2G_;
    GlobalTensor<int32_t> vG_;
    GlobalTensor<uint8_t> mG_;
};

} // namespace

extern "C" __global__ __aicore__ void f203_encrypt_g4_noise(GM_ADDR uGm, GM_ADDR e1Gm, GM_ADDR trGm, GM_ADDR e2Gm,
                                                          GM_ADDR mGm, GM_ADDR vGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() != 0) {
        return;
    }
    KernelG4Noise op;
    op.Init(uGm, e1Gm, trGm, e2Gm, mGm, vGm);
    op.Process();
}

#ifndef __CCE_KT_TEST__
void f203_encrypt_g4_noise_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *uGm, uint8_t *e1Gm,
                              uint8_t *trGm, uint8_t *e2Gm, uint8_t *mGm, uint8_t *vGm)
{
    f203_encrypt_g4_noise<<<blockDim, l2ctrl, stream>>>(uGm, e1Gm, trGm, e2Gm, mGm, vGm);
}
#endif
