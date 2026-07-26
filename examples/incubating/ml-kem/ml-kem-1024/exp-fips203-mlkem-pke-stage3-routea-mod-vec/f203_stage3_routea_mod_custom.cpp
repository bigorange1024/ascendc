/**
 * @file f203_stage3_routea_mod_custom.cpp
 * @brief 【预研】F203 Stage3 向量：mat_c [16,512] int32 → out [8,256] int32。
 *
 * 流水线位置：Stage2 MMAD 之后；RouteA 偶奇列合并 + mod q。
 * 规格书：exp-fips203-mlkem-pke-stage3-routea-mod-vec-实现方案-customspec.tex
 * - 向量：TPipe/TQue + DataCopy 流水搬运对列交错块；
 * - 标量（A2 API 缺口）：int64 RouteA 合并 + mod q（CANN 9.0.0 单次 rem）。
 * Launch：aiv=1/2/8，分核与 Stage1 同构。
 * 与 golden：out_gm.bin I/O 等价；禁止把参考源码当 AscendC 规格。
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

/**
 * 单条 poly 的 Stage3 流水：读 mat_c hi/lo 行，写 out 一行。
 */
class KernelF203Stage3RouteAMod {
public:
    __aicore__ inline KernelF203Stage3RouteAMod() {}

    /**
     * 绑定 poly p 的 GM 视图与 UB 队列。
     * @param matCGm 全局 mat_c [16,512] int32
     * @param outGm  全局 out [8,256] int32
     * @param polyIdx 0..7；hi 行=p，lo 行=8+p
     */
    __aicore__ inline void InitForPoly(GM_ADDR matCGm, GM_ADDR outGm, int32_t polyIdx)
    {
        polyIdx_ = polyIdx;
        const int32_t hiRow = polyIdx_;
        const int32_t loRow = kKPolys + polyIdx_;

        // 整幅 mat_c；按行基址 + 列偏移取 tile
        matCGlobal_.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(matCGm), kKPolys * 2 * kOutCols);
        outGlobal_.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(outGm) + polyIdx_ * kN, kN);

        hiRowBase_ = hiRow * kOutCols;
        loRowBase_ = loRow * kOutCols;

        // 双缓冲：hi/lo 各 kPairCols=32 int32；输出 kTileLen=16
        pipe_.InitBuffer(hiInQueue_, kBufferNum, static_cast<uint32_t>(kPairCols * sizeof(int32_t)));
        pipe_.InitBuffer(loInQueue_, kBufferNum, static_cast<uint32_t>(kPairCols * sizeof(int32_t)));
        pipe_.InitBuffer(outQueue_, kBufferNum, static_cast<uint32_t>(kTileLen * sizeof(int32_t)));
    }

    /**
     * 主循环：CopyIn → Compute → CopyOut，共 kTileNum*kBufferNum 个 tile。
     */
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
    /** 从 mat_c hi/lo 行拉取第 progress 个偶奇对列块到 VECIN。 */
    __aicore__ inline void CopyIn(int32_t progress)
    {
        // 每 tile 覆盖 kTileLen 个输出系数 → 需 2*kTileLen 列（偶奇交错）
        const int32_t colOffset = progress * kTileLen * 2;
        AscendC::LocalTensor<int32_t> hiPairs = hiInQueue_.AllocTensor<int32_t>();
        AscendC::LocalTensor<int32_t> loPairs = loInQueue_.AllocTensor<int32_t>();
        AscendC::DataCopy(hiPairs, matCGlobal_[hiRowBase_ + colOffset], kPairCols);
        AscendC::DataCopy(loPairs, matCGlobal_[loRowBase_ + colOffset], kPairCols);
        hiInQueue_.EnQue(hiPairs);
        loInQueue_.EnQue(loPairs);
    }

    /**
     * RouteA：raw = hh*4096 + (hl+lh)*64 + ll，再 Stage31ModI64。
     * 背景：A2 缺 int64 向量 API，本段标量循环（见 customspec §覆盖率）。
     */
    __aicore__ inline void Compute()
    {
        AscendC::LocalTensor<int32_t> hiPairs = hiInQueue_.DeQue<int32_t>();
        AscendC::LocalTensor<int32_t> loPairs = loInQueue_.DeQue<int32_t>();
        AscendC::LocalTensor<int32_t> outLocal = outQueue_.AllocTensor<int32_t>();

        // 逐系数：hiPairs[2j/2j+1]=hh/lh，loPairs[2j/2j+1]=hl/ll
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

    /** 将本 tile 的 out 写回 GM 行内偏移 progress*kTileLen。 */
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

/**
 * 设备核入口：AIV_ONLY；按 blockNum 分核处理 poly。
 * @param matCGm 输入 mat_c；@param outGm 输出 out
 * 前置：blockDim∈{1,2,8}；blockIdx < blockNum
 */
extern "C" __global__ __aicore__ void f203_stage3_routea_mod_custom(GM_ADDR matCGm, GM_ADDR outGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const int32_t blockIdx = static_cast<int32_t>(AscendC::GetBlockIdx());
    const int32_t blockNum = static_cast<int32_t>(AscendC::GetBlockNum());
    if (blockIdx >= blockNum || blockNum <= 0) {
        return;
    }

    // blockDim=1：单核串行 8 poly；否则每核 polysPerBlock 条
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
/** NPU launch 包装：blockDim 由 Host 按 LAUNCH_PROFILE 传入。 */
void f203_stage3_routea_mod_do(uint32_t blockDim, void *stream, uint8_t *matC, uint8_t *out)
{
    f203_stage3_routea_mod_custom<<<blockDim, nullptr, stream>>>(matC, out);
}
#endif
