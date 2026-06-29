/**
 * @file f203_stage3_routea_mod_custom.cpp
 * 【预研】F203 Stage3 向量预研：mat_c [16,512] int32 → out [8,256] int32。
 *
 * 规格书：exp-mlkem-f203-stage3-routea-mod-vec-实现方案-customspec.tex
 * - 向量：TPipe/TQue + DataCopy 流水搬运对列交错块；
 * - 标量（A2 API 缺口）：int64 RouteA 合并 + mod q（CANN 9.0.0 单次 rem，见 customspec §Stage3.1）。
 * - Launch：aiv=1/2/8，分核逻辑与 Stage1 同构。
 */
#include "kernel_operator.h"

namespace {
constexpr int32_t kKPolys = 8;
constexpr int32_t kN = 256;
constexpr int32_t kOutCols = 512;
constexpr int32_t kQ = 3329;
constexpr int32_t kTileNum = 8;
constexpr int32_t kBufferNum = 2;
constexpr int32_t kTileLen = kN / kTileNum / kBufferNum; // 16
constexpr int32_t kPairCols = kTileLen * 2;              // 32 int32 per row chunk
} // namespace

/**
 * int64 标量 mod q：floor 除法得 rem。
 * 背景：ntt_study 时代 Div 向量实现曾有底层问题，交付 ONNX 保留双校正归一步骤；
 * 本工程基于 CANN 9.0.0，假定整除语义正确，数学上 rem 已在 [0,q)。
 */
__aicore__ inline int32_t Stage31ModI64(int64_t raw)
{
    const int64_t q = kQ;
    const int64_t t = (raw >= 0) ? (raw / q) : (-((-raw) / q));
    return static_cast<int32_t>(raw - q * t);
}

class KernelF203Stage3RouteAMod {
public:
    __aicore__ inline KernelF203Stage3RouteAMod() {}

    /** 绑定 poly p：读 mat_c 行 p / 8+p，写 out 行 p */
    __aicore__ inline void InitForPoly(GM_ADDR matCGm, GM_ADDR outGm, int32_t polyIdx)
    {
        polyIdx_ = polyIdx;
        const int32_t hiRow = polyIdx_;
        const int32_t loRow = kKPolys + polyIdx_;

        matCGlobal_.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(matCGm), kKPolys * 2 * kOutCols);
        outGlobal_.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(outGm) + polyIdx_ * kN, kN);

        hiRowBase_ = hiRow * kOutCols;
        loRowBase_ = loRow * kOutCols;

        pipe_.InitBuffer(hiInQueue_, kBufferNum, static_cast<uint32_t>(kPairCols * sizeof(int32_t)));
        pipe_.InitBuffer(loInQueue_, kBufferNum, static_cast<uint32_t>(kPairCols * sizeof(int32_t)));
        pipe_.InitBuffer(outQueue_, kBufferNum, static_cast<uint32_t>(kTileLen * sizeof(int32_t)));
    }

    __aicore__ inline void Process()
    {
        const int32_t loopCount = kTileNum * kBufferNum;
        for (int32_t i = 0; i < loopCount; i++) {
            CopyIn(i);
            Compute();
            CopyOut(i);
        }
    }

private:
    __aicore__ inline void CopyIn(int32_t progress)
    {
        const int32_t colOffset = progress * kTileLen * 2;
        AscendC::LocalTensor<int32_t> hiPairs = hiInQueue_.AllocTensor<int32_t>();
        AscendC::LocalTensor<int32_t> loPairs = loInQueue_.AllocTensor<int32_t>();
        AscendC::DataCopy(hiPairs, matCGlobal_[hiRowBase_ + colOffset], kPairCols);
        AscendC::DataCopy(loPairs, matCGlobal_[loRowBase_ + colOffset], kPairCols);
        hiInQueue_.EnQue(hiPairs);
        loInQueue_.EnQue(loPairs);
    }

    __aicore__ inline void Compute()
    {
        AscendC::LocalTensor<int32_t> hiPairs = hiInQueue_.DeQue<int32_t>();
        AscendC::LocalTensor<int32_t> loPairs = loInQueue_.DeQue<int32_t>();
        AscendC::LocalTensor<int32_t> outLocal = outQueue_.AllocTensor<int32_t>();

        // 标量缺口：int64 RouteA 合并 + mod（见 customspec §覆盖率）
        for (int32_t j = 0; j < kTileLen; j++) {
            const int64_t hh = static_cast<int64_t>(hiPairs.GetValue(j * 2));
            const int64_t lh = static_cast<int64_t>(hiPairs.GetValue(j * 2 + 1));
            const int64_t hl = static_cast<int64_t>(loPairs.GetValue(j * 2));
            const int64_t ll = static_cast<int64_t>(loPairs.GetValue(j * 2 + 1));
            const int64_t raw = hh * 4096 + (hl + lh) * 64 + ll;
            outLocal.SetValue(j, Stage31ModI64(raw));
        }

        outQueue_.EnQue(outLocal);
        hiInQueue_.FreeTensor(hiPairs);
        loInQueue_.FreeTensor(loPairs);
    }

    __aicore__ inline void CopyOut(int32_t progress)
    {
        const int32_t colOffset = progress * kTileLen;
        AscendC::LocalTensor<int32_t> outLocal = outQueue_.DeQue<int32_t>();
        AscendC::DataCopy(outGlobal_[colOffset], outLocal, kTileLen);
        outQueue_.FreeTensor(outLocal);
    }

private:
    int32_t polyIdx_;
    int32_t hiRowBase_;
    int32_t loRowBase_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::QuePosition::VECIN, kBufferNum> hiInQueue_;
    AscendC::TQue<AscendC::QuePosition::VECIN, kBufferNum> loInQueue_;
    AscendC::TQue<AscendC::QuePosition::VECOUT, kBufferNum> outQueue_;
    AscendC::GlobalTensor<int32_t> matCGlobal_;
    AscendC::GlobalTensor<int32_t> outGlobal_;
};

extern "C" __global__ __aicore__ void f203_stage3_routea_mod_custom(GM_ADDR matCGm, GM_ADDR outGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const int32_t blockIdx = static_cast<int32_t>(AscendC::GetBlockIdx());
    const int32_t blockNum = static_cast<int32_t>(AscendC::GetBlockNum());
    if (blockIdx >= blockNum || blockNum <= 0) {
        return;
    }

    if (blockNum == 1) {
        for (int32_t p = 0; p < kKPolys; p++) {
            KernelF203Stage3RouteAMod op;
            op.InitForPoly(matCGm, outGm, p);
            op.Process();
        }
    } else {
        const int32_t polysPerBlock = kKPolys / blockNum;
        const int32_t startPoly = blockIdx * polysPerBlock;
        for (int32_t p = startPoly; p < startPoly + polysPerBlock; p++) {
            KernelF203Stage3RouteAMod op;
            op.InitForPoly(matCGm, outGm, p);
            op.Process();
        }
    }
}

#ifndef ASCENDC_CPU_DEBUG
void f203_stage3_routea_mod_do(uint32_t blockDim, void *stream, uint8_t *matC, uint8_t *out)
{
    f203_stage3_routea_mod_custom<<<blockDim, nullptr, stream>>>(matC, out);
}
#endif
