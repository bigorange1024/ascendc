#ifndef F203_AIV_ENCODE_HPP
#define F203_AIV_ENCODE_HPP

#include "kernel_operator.h"

namespace f203_encode {
constexpr int32_t kKPolys = 8;
constexpr int32_t kN = 256;
constexpr int32_t kTileNum = 4;
constexpr int32_t kBufferNum = 2;
constexpr int32_t kTileLength = kN / kTileNum / kBufferNum;
constexpr int32_t kLimb6Bits = 6;

class KernelF203Stage1Encode {
public:
    __aicore__ inline KernelF203Stage1Encode() {}

    __aicore__ inline void InitForPoly(GM_ADDR seGm, GM_ADDR matAGm, int32_t polyIdx)
    {
        const int32_t seOffset = polyIdx * kN;
        const int32_t hiRowOffset = polyIdx * kN;
        const int32_t loRowOffset = (kKPolys + polyIdx) * kN;

        seGlobal_.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(seGm) + seOffset, kN);
        hiGlobal_.SetGlobalBuffer(reinterpret_cast<__gm__ int8_t *>(matAGm) + hiRowOffset, kN);
        loGlobal_.SetGlobalBuffer(reinterpret_cast<__gm__ int8_t *>(matAGm) + loRowOffset, kN);

        pipe_.InitBuffer(inQueue_, kBufferNum, kTileLength * sizeof(int32_t));
        pipe_.InitBuffer(hiOutQueue_, kBufferNum, kTileLength * sizeof(int8_t));
        pipe_.InitBuffer(loOutQueue_, kBufferNum, kTileLength * sizeof(int8_t));
        pipe_.InitBuffer(calcBuf_, static_cast<uint32_t>(kTileLength * sizeof(int32_t) * 2 +
                                                          kTileLength * sizeof(int16_t) +
                                                          kTileLength * sizeof(half)));
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
    __aicore__ inline void CastI32ToI8(AscendC::LocalTensor<int8_t> &dst, AscendC::LocalTensor<int32_t> &src,
                                        AscendC::LocalTensor<int16_t> &tmpI16, AscendC::LocalTensor<half> &tmpHalf)
    {
        AscendC::Cast(tmpI16, src, AscendC::RoundMode::CAST_NONE, kTileLength);
        AscendC::Cast(tmpHalf, tmpI16, AscendC::RoundMode::CAST_NONE, kTileLength);
        AscendC::Cast(dst, tmpHalf, AscendC::RoundMode::CAST_NONE, kTileLength);
    }

    __aicore__ inline void CopyIn(int32_t progress)
    {
        AscendC::LocalTensor<int32_t> vLocal = inQueue_.AllocTensor<int32_t>();
        AscendC::DataCopy(vLocal, seGlobal_[progress * kTileLength], kTileLength);
        inQueue_.EnQue(vLocal);
    }

    __aicore__ inline void Compute()
    {
        constexpr uint32_t kI32TileBytes = static_cast<uint32_t>(kTileLength * sizeof(int32_t));
        constexpr uint32_t kI16TileBytes = static_cast<uint32_t>(kTileLength * sizeof(int16_t));

        AscendC::LocalTensor<int32_t> vLocal = inQueue_.DeQue<int32_t>();
        AscendC::LocalTensor<int32_t> hiLocal = calcBuf_.Get<int32_t>(kTileLength);
        AscendC::LocalTensor<int32_t> loLocal = calcBuf_.GetWithOffset<int32_t>(kTileLength, kI32TileBytes);
        AscendC::LocalTensor<int16_t> tmpI16 = calcBuf_.GetWithOffset<int16_t>(kTileLength, kI32TileBytes * 2);
        AscendC::LocalTensor<half> tmpHalf =
            calcBuf_.GetWithOffset<half>(kTileLength, kI32TileBytes * 2 + kI16TileBytes);

        AscendC::ShiftRight(hiLocal, vLocal, static_cast<int32_t>(kLimb6Bits), kTileLength);
        AscendC::Muls(loLocal, hiLocal, static_cast<int32_t>(64), kTileLength);
        AscendC::Sub(loLocal, vLocal, loLocal, kTileLength);

        AscendC::LocalTensor<int8_t> hiI8 = hiOutQueue_.AllocTensor<int8_t>();
        AscendC::LocalTensor<int8_t> loI8 = loOutQueue_.AllocTensor<int8_t>();
        CastI32ToI8(hiI8, hiLocal, tmpI16, tmpHalf);
        CastI32ToI8(loI8, loLocal, tmpI16, tmpHalf);

        hiOutQueue_.EnQue(hiI8);
        loOutQueue_.EnQue(loI8);
        inQueue_.FreeTensor(vLocal);
    }

    __aicore__ inline void CopyOut(int32_t progress)
    {
        const int32_t colOffset = progress * kTileLength;
        AscendC::LocalTensor<int8_t> hiI8 = hiOutQueue_.DeQue<int8_t>();
        AscendC::LocalTensor<int8_t> loI8 = loOutQueue_.DeQue<int8_t>();
        AscendC::DataCopy(hiGlobal_[colOffset], hiI8, kTileLength);
        AscendC::DataCopy(loGlobal_[colOffset], loI8, kTileLength);
        hiOutQueue_.FreeTensor(hiI8);
        loOutQueue_.FreeTensor(loI8);
    }

    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::QuePosition::VECIN, kBufferNum> inQueue_;
    AscendC::TQue<AscendC::QuePosition::VECOUT, kBufferNum> hiOutQueue_;
    AscendC::TQue<AscendC::QuePosition::VECOUT, kBufferNum> loOutQueue_;
    AscendC::TBuf<AscendC::TPosition::VECCALC> calcBuf_;
    AscendC::GlobalTensor<int32_t> seGlobal_;
    AscendC::GlobalTensor<int8_t> hiGlobal_;
    AscendC::GlobalTensor<int8_t> loGlobal_;
};

/** 双 AIV：subBlock 0 → poly 0..3，subBlock 1 → poly 4..7 */
__aicore__ inline void RunEncodeRange(GM_ADDR seGm, GM_ADDR matAGm, int32_t subBlockID)
{
    const int32_t polysPerSub = kKPolys / 2;
    const int32_t startPoly = subBlockID * polysPerSub;
    for (int32_t p = startPoly; p < startPoly + polysPerSub; p++) {
        KernelF203Stage1Encode op;
        op.InitForPoly(seGm, matAGm, p);
        op.Process();
    }
}
} // namespace f203_encode

#endif
