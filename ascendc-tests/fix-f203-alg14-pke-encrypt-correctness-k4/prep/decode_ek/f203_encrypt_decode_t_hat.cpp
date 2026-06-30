/**
 * @file f203_encrypt_decode_t_hat.cpp
 * @brief G5：设备侧 ByteDecode₁₂ — ek_pke[0:1536] → t_hat；并写 Â 列 0 编码到 aCol0（SIM G3）。
 *
 * 对齐 FIPS 203 Alg.6 d=12 / ML-KEM poly_frombytes；Host 不再写 input/t_hat.bin。
 */
#include "kernel_operator.h"
#include "f203_encrypt_layout.h"

using namespace AscendC;

namespace {

constexpr int32_t kK = static_cast<int32_t>(F203_ENCRYPT_K);
constexpr int32_t kN = static_cast<int32_t>(F203_ENCRYPT_N);
constexpr int32_t kPolyBytes = 384;
constexpr int32_t kEkTBytes = static_cast<int32_t>(F203_EK_T_BYTES);
constexpr int32_t kUseCores = 1;

/**
 * 单 poly ByteDecode₁₂（标量，128 pair）；与 host_golden/decode_t_hat.py 一致。
 * @param outLocal 输出 [256] int32 UB
 * @param inLocal  输入 [384] uint8 UB
 */
__aicore__ inline void poly_byte_decode12_local(LocalTensor<int32_t> &outLocal, LocalTensor<uint8_t> &inLocal)
{
    for (int32_t i = 0; i < kN / 2; ++i) {
        const int32_t b0 = static_cast<int32_t>(inLocal.GetValue(static_cast<uint32_t>(3 * i)));
        const int32_t b1 = static_cast<int32_t>(inLocal.GetValue(static_cast<uint32_t>(3 * i + 1)));
        const int32_t b2 = static_cast<int32_t>(inLocal.GetValue(static_cast<uint32_t>(3 * i + 2)));
        const int32_t t0 = b0 | ((b1 & 0x0F) << 8);
        const int32_t t1 = (b1 >> 4) | (b2 << 4);
        outLocal.SetValue(static_cast<uint32_t>(2 * i), t0);
        outLocal.SetValue(static_cast<uint32_t>(2 * i + 1), t1);
    }
}

class KernelDecodeTHat {
public:
    __aicore__ inline KernelDecodeTHat() {}

    __aicore__ inline void Init(GM_ADDR ekGm, GM_ADDR tHatGm, GM_ADDR aCol0Gm)
    {
        ekGm_ = ekGm;
        tHatGm_ = tHatGm;
        aCol0Gm_ = aCol0Gm;
        ekIn_.SetGlobalBuffer((__gm__ uint8_t *)ekGm, static_cast<uint32_t>(kEkTBytes));
        tOut_.SetGlobalBuffer((__gm__ int32_t *)tHatGm, static_cast<uint32_t>(kK * kN));
        aOut_.SetGlobalBuffer((__gm__ int32_t *)aCol0Gm, static_cast<uint32_t>(kK * kK * kN));
#if !defined(ASCENDC_CPU_DEBUG)
        pipe_.InitBuffer(inQue_, 1, static_cast<uint32_t>(kPolyBytes));
        pipe_.InitBuffer(outQue_, 1, static_cast<uint32_t>(kN * sizeof(int32_t)));
#endif
    }

    __aicore__ inline void Process()
    {
        for (int32_t j = 0; j < kK; ++j) {
#if !defined(ASCENDC_CPU_DEBUG)
            LocalTensor<uint8_t> inLocal = inQue_.AllocTensor<uint8_t>();
            LocalTensor<int32_t> outLocal = outQue_.AllocTensor<int32_t>();
            const uint32_t srcOff = static_cast<uint32_t>(j) * static_cast<uint32_t>(kPolyBytes);
            DataCopy(inLocal, ekIn_[srcOff], kPolyBytes);
            inQue_.EnQue(inLocal);
            inLocal = inQue_.DeQue<uint8_t>();
            poly_byte_decode12_local(outLocal, inLocal);
            inQue_.FreeTensor(inLocal);
            outQue_.EnQue(outLocal);
            outLocal = outQue_.DeQue<int32_t>();
            DataCopy(tOut_[static_cast<uint32_t>(j) * static_cast<uint32_t>(kN)], outLocal, kN);
            const uint32_t aOff = static_cast<uint32_t>(j * kK) * static_cast<uint32_t>(kN);
            DataCopy(aOut_[aOff], outLocal, kN);
            outQue_.FreeTensor(outLocal);
            PipeBarrier<PIPE_ALL>();
#else
            const uint8_t *ekBytes = reinterpret_cast<const uint8_t *>(ekGm_);
            int32_t *tFlat = reinterpret_cast<int32_t *>(tHatGm_);
            int32_t *aFlat = reinterpret_cast<int32_t *>(aCol0Gm_);
            uint8_t polyBuf[384];
            for (int32_t b = 0; b < kPolyBytes; ++b) {
                polyBuf[b] = ekBytes[static_cast<int32_t>(j) * kPolyBytes + b];
            }
            int32_t coeffs[256];
            for (int32_t i = 0; i < kN / 2; ++i) {
                const int32_t b0 = polyBuf[3 * i];
                const int32_t b1 = polyBuf[3 * i + 1];
                const int32_t b2 = polyBuf[3 * i + 2];
                coeffs[2 * i] = b0 | ((b1 & 0x0F) << 8);
                coeffs[2 * i + 1] = (b1 >> 4) | (b2 << 4);
            }
            for (int32_t c = 0; c < kN; ++c) {
                tFlat[j * kN + c] = coeffs[c];
                aFlat[(j * kK) * kN + c] = coeffs[c];
            }
#endif
        }
    }

private:
    GM_ADDR ekGm_{nullptr};
    GM_ADDR tHatGm_{nullptr};
    GM_ADDR aCol0Gm_{nullptr};
    TPipe pipe_;
    TQue<QuePosition::VECIN, 1> inQue_;
    TQue<QuePosition::VECOUT, 1> outQue_;
    GlobalTensor<uint8_t> ekIn_;
    GlobalTensor<int32_t> tOut_;
    GlobalTensor<int32_t> aOut_;
};

} // namespace

extern "C" __global__ __aicore__ void f203_encrypt_decode_t_hat(GM_ADDR ekGm, GM_ADDR tHatGm, GM_ADDR aCol0Gm)
{
#if defined(ASCENDC_CPU_DEBUG)
    // CPU 孪生：AIV_ONLY，与 tikicpu AIV_MODE 一致
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() >= kUseCores) {
        return;
    }
#else
    // SIM/NPU：MIX_AIC_1_2 占位，让出 AIV func_key 名额（INTEGRATION_PLAN §4 / 家里 27cc93b 方案）
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    if (AscendC::GetSubBlockNum() == 1) {
        return;
    }
    if (GetBlockIdx() >= kUseCores) {
        return;
    }
#endif
    KernelDecodeTHat op;
    op.Init(ekGm, tHatGm, aCol0Gm);
    op.Process();
}

#ifndef __CCE_KT_TEST__
void f203_encrypt_decode_t_hat_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ekGm, uint8_t *tHatGm,
                                  uint8_t *aCol0Gm)
{
    f203_encrypt_decode_t_hat<<<blockDim, l2ctrl, stream>>>(ekGm, tHatGm, aCol0Gm);
}
#endif
