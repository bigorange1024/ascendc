/**
 * @file f203_stage1_encode_custom.cpp
 * F203 Stage1 纯向量算子：se [8,256] int32 → mat_a [16,256] int8。
 *
 * 流水线：Stage2/3 之前；mat_a 行 0..7 为 HI、8..15 为 LO（与 golden encode_mat_a 一致）。
 * 写法参考 AddCustomSample（TPipe + TQue 双缓冲）；AIV_ONLY，无 Cube。
 *
 * Launch（见 launch_profile.h / run.sh --aiv）：
 *   - blockDim=1：单 Vector 核串行处理 8 条 poly
 *   - blockDim=8：8 核各处理 1 条 poly（blockIdx = poly 下标）
 *
 * 编码语义（对齐 MlkemEncodeToLimb6 / mlkem_ref）：
 *   hi = v >> 6，lo = v - hi*64（等价 v&63，避免 And 在部分路径未生效）
 * 背景：输入由 Host 保证 v ∈ [0,Q)；Q=3329，6-bit limb 足够。
 */
#include "kernel_operator.h"

namespace {
// 几何常量：与 gen_data.py、main.cpp、customspec 表一致
constexpr int32_t kKPolys = 8;   // se 行数 / HI、LO 各 8 行
constexpr int32_t kN = 256;        // 每条 poly 系数个数
constexpr int32_t kBlockLength = kN; // 单 poly 在 GM 上连续长度（元素个数）

// Tiling：整条 poly 分 4×2 块流水（AddCustom 惯例：tileNum × bufferNum 次循环）
constexpr int32_t kTileNum = 4;
constexpr int32_t kBufferNum = 2;
constexpr int32_t kTileLength = kBlockLength / kTileNum / kBufferNum; // 256/8 = 32

constexpr int32_t kLimb6Bits = 6;  // ML-KEM limb 位宽
constexpr int32_t kLimb6Mask = 63; // 保留：语义上 lo ∈ [0,63]
} // namespace

/**
 * 单条 poly 的 encode 流水：GM 读 se 一行，写 mat_a 的 HI/LO 各一行。
 * 成员在 InitForPoly 中按 polyIdx 绑定 GM 偏移与 UB 队列。
 */
class KernelF203Stage1Encode {
public:
    __aicore__ inline KernelF203Stage1Encode() {}

    /**
     * 绑定本条 poly 的 GM 视图与 UB 缓冲。
     * @param seGm    全局 se [8,256] int32 首地址
     * @param matAGm  全局 mat_a [16,256] int8 首地址
     * @param polyIdx 当前 poly 下标 0..7
     *
     * GM 布局（行优先）：
     *   se 行偏移     = polyIdx * 256
     *   mat_a HI 行   = polyIdx * 256
     *   mat_a LO 行   = (8 + polyIdx) * 256
     */
    __aicore__ inline void InitForPoly(GM_ADDR seGm, GM_ADDR matAGm, int32_t polyIdx)
    {
        polyIdx_ = polyIdx;

        const int32_t seOffset = polyIdx_ * kN;
        const int32_t hiRowOffset = polyIdx_ * kN;
        const int32_t loRowOffset = (kKPolys + polyIdx_) * kN;

        seGlobal_.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(seGm) + seOffset, kBlockLength);
        hiGlobal_.SetGlobalBuffer(reinterpret_cast<__gm__ int8_t *>(matAGm) + hiRowOffset, kBlockLength);
        loGlobal_.SetGlobalBuffer(reinterpret_cast<__gm__ int8_t *>(matAGm) + loRowOffset, kBlockLength);

        // 输入队列：int32 tile；输出队列：HI/LO 各 int8 tile
        pipe_.InitBuffer(inQueue_, kBufferNum, kTileLength * sizeof(int32_t));
        pipe_.InitBuffer(hiOutQueue_, kBufferNum, kTileLength * sizeof(int8_t));
        pipe_.InitBuffer(loOutQueue_, kBufferNum, kTileLength * sizeof(int8_t));

        // 计算区：2 块 int32（hi、lo 向量）+ int16/half 窄化中转
        // 背景：dav_c220 不支持 int32→int8 直连，须 int32→int16→half→int8
        pipe_.InitBuffer(calcBuf_, static_cast<uint32_t>(kTileLength * sizeof(int32_t) * 2 +
                                                          kTileLength * sizeof(int16_t) +
                                                          kTileLength * sizeof(half)));
    }

    /**
     * 主循环：CopyIn → Compute → CopyOut，共 kTileNum*kBufferNum 个 tile。
     * progress 与 GM 列偏移 progress*kTileLength 对齐。
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
    /** 从 GM se 拉取第 progress 个 tile 到 VECIN 队列 */
    __aicore__ inline void CopyIn(int32_t progress)
    {
        AscendC::LocalTensor<int32_t> vLocal = inQueue_.AllocTensor<int32_t>();
        AscendC::DataCopy(vLocal, seGlobal_[progress * kTileLength], kTileLength);
        inQueue_.EnQue(vLocal);
    }

    /**
     * int32 → int8 窄化链（硬件约束）。
     * @param dst     目标 int8 local（长度 kTileLength）
     * @param src     源 int32 local
     * @param tmpI16  中转 int16
     * @param tmpHalf 中转 half
     */
    __aicore__ inline void CastI32ToI8(AscendC::LocalTensor<int8_t> &dst, AscendC::LocalTensor<int32_t> &src,
                                        AscendC::LocalTensor<int16_t> &tmpI16, AscendC::LocalTensor<half> &tmpHalf)
    {
        AscendC::Cast(tmpI16, src, AscendC::RoundMode::CAST_NONE, kTileLength);
        AscendC::Cast(tmpHalf, tmpI16, AscendC::RoundMode::CAST_NONE, kTileLength);
        AscendC::Cast(dst, tmpHalf, AscendC::RoundMode::CAST_NONE, kTileLength);
    }

    /**
     * 对当前 tile：分解 6-bit limb，窄化后入 HI/LO 输出队列。
     * hiLocal/loLocal 为 int32 域结果，再 Cast 到 int8 写 GM。
     */
    __aicore__ inline void Compute()
    {
        constexpr uint32_t kI32TileBytes = static_cast<uint32_t>(kTileLength * sizeof(int32_t));
        constexpr uint32_t kI16TileBytes = static_cast<uint32_t>(kTileLength * sizeof(int16_t));

        AscendC::LocalTensor<int32_t> vLocal = inQueue_.DeQue<int32_t>();
        // calcBuf 切片：前 kTileLength 为 hi，次 kTileLength 为 lo
        AscendC::LocalTensor<int32_t> hiLocal = calcBuf_.Get<int32_t>(kTileLength);
        AscendC::LocalTensor<int32_t> loLocal = calcBuf_.GetWithOffset<int32_t>(kTileLength, kI32TileBytes);
        AscendC::LocalTensor<int16_t> tmpI16 =
            calcBuf_.GetWithOffset<int16_t>(kTileLength, kI32TileBytes * 2);
        AscendC::LocalTensor<half> tmpHalf =
            calcBuf_.GetWithOffset<half>(kTileLength, kI32TileBytes * 2 + kI16TileBytes);

        // limb 分解：hi = v>>6；lo = v - hi*64（与 ONNX Floor/Sub 一致，非 And）
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

    /** 将 HI/LO int8 tile 写回 mat_a 对应行的列偏移 */
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

private:
    int32_t polyIdx_;
    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::QuePosition::VECIN, kBufferNum> inQueue_;
    AscendC::TQue<AscendC::QuePosition::VECOUT, kBufferNum> hiOutQueue_;
    AscendC::TQue<AscendC::QuePosition::VECOUT, kBufferNum> loOutQueue_;
    AscendC::TBuf<AscendC::TPosition::VECCALC> calcBuf_;
    AscendC::GlobalTensor<int32_t> seGlobal_;
    AscendC::GlobalTensor<int8_t> hiGlobal_;
    AscendC::GlobalTensor<int8_t> loGlobal_;
};

/**
 * 核入口：按 blockDim 分派 poly。
 * @param seGm   [8,256] int32 GM
 * @param matAGm [16,256] int8 GM
 *
 * blockNum==1：单核串行 8 poly
 * blockNum==2|8：每核 8/blockNum 条 poly，核内串行；每 poly 新建 op（避免 TPipe 跨 poly 复用 abort）
 */
extern "C" __global__ __aicore__ void f203_stage1_encode_custom(GM_ADDR seGm, GM_ADDR matAGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const int32_t blockIdx = static_cast<int32_t>(AscendC::GetBlockIdx());
    const int32_t blockNum = static_cast<int32_t>(AscendC::GetBlockNum());
    if (blockIdx >= blockNum || blockNum <= 0) {
        return;
    }

    if (blockNum == 1) {
        // aiv=1：单核串行 8 poly
        for (int32_t p = 0; p < kKPolys; p++) {
            KernelF203Stage1Encode op;
            op.InitForPoly(seGm, matAGm, p);
            op.Process();
        }
    } else {
        // aiv=2（每核 4 poly）/ aiv=8（每核 1 poly）：按 block 均分 poly
        const int32_t polysPerBlock = kKPolys / blockNum;
        const int32_t startPoly = blockIdx * polysPerBlock;
        for (int32_t p = startPoly; p < startPoly + polysPerBlock; p++) {
            KernelF203Stage1Encode op;
            op.InitForPoly(seGm, matAGm, p);
            op.Process();
        }
    }
}

#ifndef ASCENDC_CPU_DEBUG
/** NPU 侧 launch 包装：blockDim 由 main 根据 LAUNCH_PROFILE 传入 */
void f203_stage1_encode_do(uint32_t blockDim, void *stream, uint8_t *se, uint8_t *matA)
{
    f203_stage1_encode_custom<<<blockDim, nullptr, stream>>>(se, matA);
}
#endif
