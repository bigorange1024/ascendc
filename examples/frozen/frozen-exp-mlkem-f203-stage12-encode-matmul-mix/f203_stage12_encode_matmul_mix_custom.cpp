/**
 * @file f203_stage12_encode_matmul_mix_custom.cpp
 * F203 Stage1+2 融合：se [8,256] int32 + LUT [256,512] int8 → mat_c [16,512] int32。
 *
 * 规格书：exp-mlkem-f203-stage12-encode-matmul-mix-实现方案-customspec.tex
 * - MIX 1 AIC + 2 AIV；blockDim=1（aicore=1）
 * - userWorkspace：mat_a@0，mat_c@4096
 * - Stage1 AIV encode → CrossCore → Stage2 AIC Matmul
 *
 * Stage1 tile：256=4×64，对齐 sepolyvec8 / Vector 搬运粒度。
 */
#include "kernel_operator.h"
#include "lib/matmul_intf.h"

using namespace matmul;

namespace {
constexpr int32_t kKPolys = 8;
constexpr int32_t kN = 256;
constexpr int32_t kRowsA = 16;
constexpr int32_t kOutCols = 512;
constexpr uint32_t kMatABytes = static_cast<uint32_t>(kRowsA * kN);
constexpr uint32_t kMatCBytes = static_cast<uint32_t>(kRowsA * kOutCols * sizeof(int32_t));

constexpr int32_t kStage1TileLen = 64;
constexpr int32_t kStage1TilesPerRow = kN / kStage1TileLen;
constexpr int32_t kStage1BufferNum = 1;
constexpr int32_t kLimb6Bits = 6;

constexpr uint64_t kSyncStage1Done = 7;
} // namespace

__aicore__ inline uint32_t Ceiling(uint32_t a, uint32_t b)
{
    return (a + b - 1) / b;
}

__aicore__ inline void CopyTiling(TCubeTiling *tiling, uint64_t &localMemSize, GM_ADDR tilingGM)
{
    uint32_t *ptr = reinterpret_cast<uint32_t *>(tiling);
    auto tiling32 = reinterpret_cast<__gm__ uint32_t *>(tilingGM);
    for (uint32_t i = 0; i < sizeof(TCubeTiling) / sizeof(uint32_t); i++, ptr++) {
        *ptr = *(tiling32 + i);
    }
    localMemSize = *reinterpret_cast<__gm__ uint64_t *>(tilingGM + sizeof(TCubeTiling));
}

__aicore__ inline void CalcGMOffset(int blockIdx, const TCubeTiling &tiling, int &offsetA, int &offsetB, int &offsetC,
                                    int &tailM, int &tailN, bool isTransA, bool isTransB)
{
    const uint32_t mSingleBlocks = Ceiling(tiling.M, tiling.singleCoreM);
    const uint32_t mCoreIndx = blockIdx % mSingleBlocks;
    const uint32_t nCoreIndx = blockIdx / mSingleBlocks;

    offsetA = mCoreIndx * tiling.Ka * tiling.singleCoreM;
    if (isTransA) {
        offsetA = mCoreIndx * tiling.singleCoreM;
    }
    offsetB = nCoreIndx * tiling.singleCoreN;
    if (isTransB) {
        offsetB = nCoreIndx * tiling.Kb * tiling.singleCoreN;
    }
    offsetC = mCoreIndx * tiling.N * tiling.singleCoreM + nCoreIndx * tiling.singleCoreN;

    tailM = tiling.M - mCoreIndx * tiling.singleCoreM;
    tailM = tailM < tiling.singleCoreM ? tailM : tiling.singleCoreM;

    tailN = tiling.N - nCoreIndx * tiling.singleCoreN;
    tailN = tailN < tiling.singleCoreN ? tailN : tiling.singleCoreN;
}

/**
 * Stage1 向量 encode（AIV）：se → workspace mat_a。
 * 语义：hi=v>>6，lo=v-hi*64；Cast 链 i32→i16→half→i8。
 */
class Stage1EncodeMatAVector {
public:
    __aicore__ inline Stage1EncodeMatAVector() {}

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

    /** 8 poly × 4 tile，核内串行（对齐 sepolyvec8 Stage1） */
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

private:
    AscendC::TPipe *pipe_;
    AscendC::GlobalTensor<int32_t> seGlobal_;
    AscendC::GlobalTensor<int8_t> matAGlobal_;
    AscendC::TQue<AscendC::TPosition::VECIN, kStage1BufferNum> inQueue_;
    AscendC::TQue<AscendC::TPosition::VECOUT, kStage1BufferNum> hiOutQueue_;
    AscendC::TQue<AscendC::TPosition::VECOUT, kStage1BufferNum> loOutQueue_;
    AscendC::TBuf<AscendC::TPosition::VECCALC> calcBuf_;
};

/** Stage2 Cube MatMul（AIC）：mat_a × lut → mat_c */
__aicore__ inline void Stage2MatmulCube(GM_ADDR matAGm, GM_ADDR lutGm, GM_ADDR matCGm, GM_ADDR tilingGm,
                                        AscendC::TPipe *pipe)
{
    using A_T = int8_t;
    using B_T = int8_t;
    using C_T = int32_t;

    TCubeTiling tiling;
    uint64_t localMemSize = 0;
    CopyTiling(&tiling, localMemSize, tilingGm);

    AscendC::GlobalTensor<A_T> aGlobal;
    AscendC::GlobalTensor<B_T> bGlobal;
    AscendC::GlobalTensor<C_T> cGlobal;
    aGlobal.SetGlobalBuffer(reinterpret_cast<__gm__ A_T *>(matAGm), tiling.M * tiling.Ka);
    bGlobal.SetGlobalBuffer(reinterpret_cast<__gm__ B_T *>(lutGm), tiling.Kb * tiling.N);
    cGlobal.SetGlobalBuffer(reinterpret_cast<__gm__ C_T *>(matCGm), tiling.M * tiling.N);

    int offsetA = 0;
    int offsetB = 0;
    int offsetC = 0;
    const bool isTransA = false;
    const bool isTransB = false;
    int tailM = 0;
    int tailN = 0;
    CalcGMOffset(AscendC::GetBlockIdx(), tiling, offsetA, offsetB, offsetC, tailM, tailN, isTransA, isTransB);

    auto gmA = aGlobal[offsetA];
    auto gmB = bGlobal[offsetB];
    auto gmC = cGlobal[offsetC];

    Matmul<MatmulType<AscendC::TPosition::GM, CubeFormat::ND, A_T>,
           MatmulType<AscendC::TPosition::GM, CubeFormat::ND, B_T>,
           MatmulType<AscendC::TPosition::GM, CubeFormat::ND, C_T>>
        mm;
    REGIST_MATMUL_OBJ(pipe, GetSysWorkSpacePtr(), mm, &tiling);
    (void)localMemSize;
    mm.SetTensorA(gmA, isTransA);
    mm.SetTensorB(gmB, isTransB);
    mm.SetTail(tailM, tailN);
    mm.IterateAll(gmC);
    mm.End();
}

/**
 * 仅 Stage1 encode（无 CrossCore / Matmul）。
 * 供 CPU / CaModel 串行仿真分两趟 launch：本核写 mat_a，再 launch 隔离 Stage2 Cube。
 */
extern "C" __global__ __aicore__ void f203_stage12_encode_only_custom(GM_ADDR seGm, GM_ADDR matAGm)
{
#ifndef ASCENDC_CPU_DEBUG
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
#endif
    if (AscendC::GetBlockIdx() >= AscendC::GetBlockNum()) {
        return;
    }
    if ASCEND_IS_AIV {
        if (AscendC::GetBlockIdx() == 0 && AscendC::GetSubBlockIdx() == 0) {
            AscendC::TPipe pipe;
            Stage1EncodeMatAVector stage1;
            stage1.Init(seGm, matAGm, &pipe);
            stage1.Process();
        }
    }
}

/**
 * MIX 核入口（真机单趟：encode → CrossCore → Matmul）。
 * @param seGm            [8,256] int32
 * @param lutGm           [256,512] int8
 * @param userWorkspaceGm mat_a@0 + mat_c@4096
 * @param workspaceGm     Matmul 库 workspace
 * @param tilingGm        TCubeTiling
 */
#ifdef ASCENDC_CPU_DEBUG
// CPU 串行仿真：Host 用 g_f203_stage12_host_pass 分两趟（pass1 encode / pass2 改走隔离 Cube）。
volatile int g_f203_stage12_host_pass = 1;
#endif

extern "C" __global__ __aicore__ void f203_stage12_encode_matmul_mix_custom(GM_ADDR seGm, GM_ADDR lutGm,
                                                                            GM_ADDR userWorkspaceGm, GM_ADDR workspaceGm,
                                                                            GM_ADDR tilingGm)
{
#ifndef ASCENDC_CPU_DEBUG
    // 真机 MIX 须声明 1AIC+2AIV；CPU 孪生不设此项，否则 Stage2 Matmul IterateAll 会挂死。
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
#endif

    if (AscendC::GetBlockIdx() >= AscendC::GetBlockNum()) {
        return;
    }

    AscendC::TPipe pipe;
    GM_ADDR matAGm = userWorkspaceGm;
    GM_ADDR matCGm = userWorkspaceGm + kMatABytes;

#ifdef ASCENDC_CPU_DEBUG
    const bool runStage1 = (g_f203_stage12_host_pass == 1);
    const bool runStage2 = (g_f203_stage12_host_pass == 2);
#else
    const bool runStage1 = true;
    const bool runStage2 = true;
#endif
    const bool needS1ToS2Sync = runStage1 && runStage2;

    if (runStage1) {
        if ASCEND_IS_AIV {
            if (AscendC::GetBlockIdx() == 0 && AscendC::GetSubBlockIdx() == 0) {
                Stage1EncodeMatAVector stage1;
                stage1.Init(seGm, matAGm, &pipe);
                stage1.Process();
                if (needS1ToS2Sync) {
                    AscendC::CrossCoreSetFlag<2, PIPE_MTE3>(kSyncStage1Done);
                }
            }
        }
        if (needS1ToS2Sync) {
            if ASCEND_IS_AIC {
                AscendC::CrossCoreWaitFlag(kSyncStage1Done);
            }
        }
    }

    if (runStage2) {
        if ASCEND_IS_AIC {
            Stage2MatmulCube(matAGm, lutGm, matCGm, tilingGm, &pipe);
        }
    }
    (void)workspaceGm;
}
