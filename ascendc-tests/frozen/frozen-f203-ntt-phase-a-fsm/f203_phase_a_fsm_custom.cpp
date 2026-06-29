/**
 * Phase A：F203 6bit encode + MIX CrossCore（AIC_1_2）。
 * sim 用 Stage12 式 PIPE_MTE3 单 flag；merged_kyber FSM（MTE2）在 F203 harness 上 sim 挂死。
 * 参数布局：3×GM + TilingData（ws 作 workspace，sync 写 ws[0]）。
 */
#include "kernel_operator.h"
#include "phase_a_tiling.h"

namespace {
constexpr int32_t kKPolys = 8;
constexpr int32_t kN = 256;
constexpr int32_t kRowsA = 16;
constexpr int32_t kStage1TileLen = 64;
constexpr int32_t kStage1TilesPerRow = kN / kStage1TileLen;
constexpr int32_t kStage1BufferNum = 1;
constexpr int32_t kLimb6Bits = 6;
constexpr uint64_t kSyncEncodeDone = 7;
} // namespace

class Stage1Encode6Bit {
public:
    __aicore__ inline void Init(GM_ADDR seGm, GM_ADDR matAGm, AscendC::TPipe *pipeIn)
    {
        pipe_ = pipeIn;
        seGlobal_.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(seGm), kKPolys * kN);
        matAGlobal_.SetGlobalBuffer(reinterpret_cast<__gm__ int8_t *>(matAGm), kRowsA * kN);
        pipe_->InitBuffer(inQueue_, kStage1BufferNum, kStage1TileLen * sizeof(int32_t));
        pipe_->InitBuffer(hiOutQueue_, kStage1BufferNum, kStage1TileLen * sizeof(int8_t));
        pipe_->InitBuffer(loOutQueue_, kStage1BufferNum, kStage1TileLen * sizeof(int8_t));
        pipe_->InitBuffer(calcBuf_, static_cast<uint32_t>(kStage1TileLen * sizeof(int32_t) * 2 +
                                                          kStage1TileLen * sizeof(int16_t) +
                                                          kStage1TileLen * sizeof(half)));
    }

    __aicore__ inline void Process()
    {
        for (int32_t poly = 0; poly < kKPolys; poly++) {
            for (int32_t tile = 0; tile < kStage1TilesPerRow; tile++) {
                CopyIn(poly, tile);
                Compute();
                CopyOut(poly, tile);
            }
        }
    }

private:
    __aicore__ inline void CopyIn(int32_t poly, int32_t tile)
    {
        const int32_t offset = poly * kN + tile * kStage1TileLen;
        AscendC::LocalTensor<int32_t> vLocal = inQueue_.AllocTensor<int32_t>();
        AscendC::DataCopy(vLocal, seGlobal_[offset], kStage1TileLen);
        inQueue_.EnQue(vLocal);
    }

    __aicore__ inline void CastI32ToI8(AscendC::LocalTensor<int8_t> &dst, AscendC::LocalTensor<int32_t> &src,
                                        AscendC::LocalTensor<int16_t> &tmpI16, AscendC::LocalTensor<half> &tmpHalf)
    {
        AscendC::Cast(tmpI16, src, AscendC::RoundMode::CAST_NONE, kStage1TileLen);
        AscendC::Cast(tmpHalf, tmpI16, AscendC::RoundMode::CAST_NONE, kStage1TileLen);
        AscendC::Cast(dst, tmpHalf, AscendC::RoundMode::CAST_NONE, kStage1TileLen);
    }

    __aicore__ inline void Compute()
    {
        constexpr uint32_t kI32TileBytes = static_cast<uint32_t>(kStage1TileLen * sizeof(int32_t));
        constexpr uint32_t kI16TileBytes = static_cast<uint32_t>(kStage1TileLen * sizeof(int16_t));
        AscendC::LocalTensor<int32_t> vLocal = inQueue_.DeQue<int32_t>();
        AscendC::LocalTensor<int32_t> hiLocal = calcBuf_.Get<int32_t>(kStage1TileLen);
        AscendC::LocalTensor<int32_t> loLocal = calcBuf_.GetWithOffset<int32_t>(kStage1TileLen, kI32TileBytes);
        AscendC::LocalTensor<int16_t> tmpI16 =
            calcBuf_.GetWithOffset<int16_t>(kStage1TileLen, kI32TileBytes * 2);
        AscendC::LocalTensor<half> tmpHalf =
            calcBuf_.GetWithOffset<half>(kStage1TileLen, kI32TileBytes * 2 + kI16TileBytes);

        AscendC::ShiftRight(hiLocal, vLocal, static_cast<int32_t>(kLimb6Bits), kStage1TileLen);
        AscendC::Muls(loLocal, hiLocal, static_cast<int32_t>(64), kStage1TileLen);
        AscendC::Sub(loLocal, vLocal, loLocal, kStage1TileLen);

        AscendC::LocalTensor<int8_t> hiI8 = hiOutQueue_.AllocTensor<int8_t>();
        AscendC::LocalTensor<int8_t> loI8 = loOutQueue_.AllocTensor<int8_t>();
        CastI32ToI8(hiI8, hiLocal, tmpI16, tmpHalf);
        CastI32ToI8(loI8, loLocal, tmpI16, tmpHalf);
        hiOutQueue_.EnQue(hiI8);
        loOutQueue_.EnQue(loI8);
        inQueue_.FreeTensor(vLocal);
    }

    __aicore__ inline void CopyOut(int32_t poly, int32_t tile)
    {
        const int32_t colOffset = tile * kStage1TileLen;
        AscendC::LocalTensor<int8_t> hiI8 = hiOutQueue_.DeQue<int8_t>();
        AscendC::LocalTensor<int8_t> loI8 = loOutQueue_.DeQue<int8_t>();
        AscendC::DataCopy(matAGlobal_[poly * kN + colOffset], hiI8, kStage1TileLen);
        AscendC::DataCopy(matAGlobal_[(kKPolys + poly) * kN + colOffset], loI8, kStage1TileLen);
        hiOutQueue_.FreeTensor(hiI8);
        loOutQueue_.FreeTensor(loI8);
    }

    AscendC::TPipe *pipe_;
    AscendC::GlobalTensor<int32_t> seGlobal_;
    AscendC::GlobalTensor<int8_t> matAGlobal_;
    AscendC::TQue<AscendC::TPosition::VECIN, kStage1BufferNum> inQueue_;
    AscendC::TQue<AscendC::TPosition::VECOUT, kStage1BufferNum> hiOutQueue_;
    AscendC::TQue<AscendC::TPosition::VECOUT, kStage1BufferNum> loOutQueue_;
    AscendC::TBuf<AscendC::TPosition::VECCALC> calcBuf_;
};

/** AIC：WAIT encode 完成后写 sync 标记到 ws[0]（避免 sync 作第 4 个 GM 破坏 auto_gen workspace） */
__aicore__ inline void AicObserveSync(GM_ADDR wsGm)
{
    AscendC::GlobalTensor<int32_t> sync;
    sync.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(wsGm), 1);
    sync.SetValue(0, static_cast<int32_t>(0xA1C0A1C0));
}

extern "C" __global__ __aicore__ void f203_phase_a_fsm_custom(GM_ADDR seGm, GM_ADDR matAGm, GM_ADDR wsGm,
                                                              TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (AscendC::GetBlockIdx() >= AscendC::GetBlockNum()) {
        return;
    }

    (void)wsGm;
    (void)tiling;

#ifdef ASCENDC_CPU_DEBUG
    // CPU：Host 分两趟 launch（MIX AIV → AIC）；单趟内不做 CrossCore。
    if (AIC) {
        if (AscendC::GetBlockIdx() == 0) {
            AicObserveSync(wsGm);
        }
        return;
    }
    if (AscendC::GetBlockIdx() != 0 || subBlockID != 0) {
        return;
    }
    {
        AscendC::TPipe pipe;
        Stage1Encode6Bit enc;
        enc.Init(seGm, matAGm, &pipe);
        enc.Process();
    }
#else
    (void)subBlockID;
    AscendC::TPipe pipe;
    if ASCEND_IS_AIV {
        if (AscendC::GetBlockIdx() == 0 && AscendC::GetSubBlockIdx() == 0) {
            Stage1Encode6Bit enc;
            enc.Init(seGm, matAGm, &pipe);
            enc.Process();
            AscendC::CrossCoreSetFlag<2, PIPE_MTE3>(kSyncEncodeDone);
        }
    }
    if ASCEND_IS_AIC {
        AscendC::CrossCoreWaitFlag(kSyncEncodeDone);
        AicObserveSync(wsGm);
    }
#endif
}

