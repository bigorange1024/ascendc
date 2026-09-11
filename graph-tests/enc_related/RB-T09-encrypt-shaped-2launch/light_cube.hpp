/**
 * @file light_cube.hpp
 * @brief RB-T09 极轻 Cube：单趟 16×32×32 int8→int32 MMAD（有界真计算，无算法正确性要求）。
 *
 * 作用：Launch2 的 NTT 段与 INTT 段各调用一次，证明两段均为有界计算（非空转）。
 * 输入/输出：GM 上 A[m,k] int8、B[k,n] int8 → C[m,n] int32（ND）。
 * 前置：仅在 AIC 路径调用。
 *
 * API 依据：查阅索引已有 DataCopy(Nd2Nz)/LoadData/Mmad/Fixpipe（RB-T01/T03 记录）；
 * 本文件不引入新 AscendC API。
 */
#ifndef RB_T09_LIGHT_CUBE_HPP
#define RB_T09_LIGHT_CUBE_HPP

#include "kernel_operator.h"
#include "tiling.h"
#include <cstdint>

namespace rb_t09 {

static constexpr uint32_t CUBE_BLOCK_SIZE = 16 * 32;

static constexpr __aicore__ inline uint16_t ceil_div(uint16_t a, uint16_t mod)
{
    return static_cast<uint16_t>((a + mod - 1) / mod);
}

/**
 * 极轻 Cube：CopyIn(Nd2Nz) → SplitA/B → Mmad → Fixpipe。
 * 与 golden 无关；仅要求 SIM/CPU 不挂死。
 */
class LightCube {
public:
    __aicore__ inline LightCube()
    {
        m_ = tiling::kM;
        k_ = tiling::kKk;
        n_ = tiling::kN;
        aSize_ = static_cast<size_t>(m_) * k_;
        bSize_ = static_cast<size_t>(k_) * n_;
        cSize_ = static_cast<size_t>(m_) * n_;
    }

    /** 申请 A1/A2/B1/B2/CO1 各级 TQue。 */
    __aicore__ inline void Init()
    {
        pipe_.InitBuffer(inQueueA1_, 1, aSize_ * sizeof(int8_t));
        pipe_.InitBuffer(inQueueA2_, 1, aSize_ * sizeof(int8_t));
        pipe_.InitBuffer(inQueueB1_, 1, bSize_ * sizeof(int8_t));
        pipe_.InitBuffer(inQueueB2_, 1, bSize_ * sizeof(int8_t));
        pipe_.InitBuffer(outQueueCO1_, 1, cSize_ * sizeof(int32_t));
    }

    /**
     * 执行一次极轻 MMAD。
     * @param dst C 输出 GM（int32[m,n]）
     * @param a   A 输入 GM（int8[m,k]）
     * @param b   B 输入 GM（int8[k,n]）
     */
    __aicore__ inline void Process(GM_ADDR dst, GM_ADDR a, GM_ADDR b)
    {
        aGM_.SetGlobalBuffer(reinterpret_cast<__gm__ int8_t *>(a));
        bGM_.SetGlobalBuffer(reinterpret_cast<__gm__ int8_t *>(b));
        cGM_.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(dst));
        CopyIn();
        SplitA();
        SplitB();
        Compute();
        CopyOut();
    }

private:
    __aicore__ inline void CopyIn()
    {
        AscendC::LocalTensor<int8_t> a1 = inQueueA1_.AllocTensor<int8_t>();
        AscendC::LocalTensor<int8_t> b1 = inQueueB1_.AllocTensor<int8_t>();

        AscendC::Nd2NzParams pa;
        pa.ndNum = 1;
        pa.nValue = m_;
        pa.dValue = k_;
        pa.srcNdMatrixStride = 0;
        pa.srcDValue = k_;
        pa.dstNzC0Stride = ceil_div(m_, 16) * 16;
        pa.dstNzNStride = 1;
        pa.dstNzMatrixStride = 0;
        AscendC::DataCopy(a1, aGM_, pa);

        AscendC::Nd2NzParams pb;
        pb.ndNum = 1;
        pb.nValue = k_;
        pb.dValue = n_;
        pb.srcNdMatrixStride = 0;
        pb.srcDValue = n_;
        pb.dstNzC0Stride = ceil_div(k_, 16) * 16;
        pb.dstNzNStride = 1;
        pb.dstNzMatrixStride = 0;
        AscendC::DataCopy(b1, bGM_, pb);

        inQueueA1_.EnQue(a1);
        inQueueB1_.EnQue(b1);
    }

    __aicore__ inline void SplitA()
    {
        AscendC::LocalTensor<int8_t> a1 = inQueueA1_.DeQue<int8_t>();
        AscendC::LocalTensor<int8_t> a2 = inQueueA2_.AllocTensor<int8_t>();
        uint32_t dstOffset = ceil_div(k_, 32) * CUBE_BLOCK_SIZE;
        uint32_t srcOffset = CUBE_BLOCK_SIZE;
        AscendC::LoadData2dParams lp;
        lp.repeatTimes = ceil_div(k_, 32);
        lp.srcStride = ceil_div(m_, 16);
        lp.dstGap = 0;
        lp.ifTranspose = false;
        for (int32_t i = 0; i < ceil_div(m_, 16); ++i) {
            AscendC::LoadData(a2[i * dstOffset], a1[i * srcOffset], lp);
        }
        inQueueA1_.FreeTensor(a1);
        inQueueA2_.EnQue<int8_t>(a2);
    }

    __aicore__ inline void SplitB()
    {
        AscendC::LocalTensor<int8_t> b1 = inQueueB1_.DeQue<int8_t>();
        AscendC::LocalTensor<int8_t> b2 = inQueueB2_.AllocTensor<int8_t>();
        uint32_t dstOffset = ceil_div(n_, 32) * (2 * CUBE_BLOCK_SIZE);
        uint32_t srcOffset = 2 * CUBE_BLOCK_SIZE;
        AscendC::LoadData2dTransposeParams lp;
        lp.repeatTimes = ceil_div(n_, 32);
        lp.srcStride = ceil_div(k_, 32);
        lp.dstGap = 1;
        lp.dstFracGap = 0;
        for (int32_t i = 0; i < ceil_div(k_, 32); ++i) {
            AscendC::LoadDataWithTranspose(b2[i * dstOffset], b1[i * srcOffset], lp);
        }
        inQueueB1_.FreeTensor(b1);
        inQueueB2_.EnQue<int8_t>(b2);
    }

    __aicore__ inline void Compute()
    {
        AscendC::LocalTensor<int8_t> a2 = inQueueA2_.DeQue<int8_t>();
        AscendC::LocalTensor<int8_t> b2 = inQueueB2_.DeQue<int8_t>();
        AscendC::LocalTensor<int32_t> c1 = outQueueCO1_.AllocTensor<int32_t>();
        AscendC::MmadParams mp;
        mp.m = ceil_div(m_, 16) * 16;
        mp.k = k_;
        mp.n = n_;
        mp.cmatrixInitVal = true;
        AscendC::Mmad(c1, a2, b2, mp);
        outQueueCO1_.EnQue<int32_t>(c1);
        inQueueA2_.FreeTensor(a2);
        inQueueB2_.FreeTensor(b2);
    }

    __aicore__ inline void CopyOut()
    {
        AscendC::LocalTensor<int32_t> c1 = outQueueCO1_.DeQue<int32_t>();
        AscendC::FixpipeParamsV220 fp;
        fp.nSize = n_;
        fp.mSize = m_;
        fp.srcStride = ceil_div(m_, 16) * 16;
        fp.dstStride = n_;
        fp.ndNum = 1;
        fp.srcNdStride = 0;
        fp.dstNdStride = 0;
        AscendC::Fixpipe(cGM_, c1, fp);
        outQueueCO1_.FreeTensor(c1);
    }

    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::A1, 1> inQueueA1_;
    AscendC::TQue<AscendC::TPosition::A2, 1> inQueueA2_;
    AscendC::TQue<AscendC::TPosition::B1, 1> inQueueB1_;
    AscendC::TQue<AscendC::TPosition::B2, 1> inQueueB2_;
    AscendC::TQue<AscendC::TPosition::CO1, 1> outQueueCO1_;
    AscendC::GlobalTensor<int8_t> aGM_;
    AscendC::GlobalTensor<int8_t> bGM_;
    AscendC::GlobalTensor<int32_t> cGM_;
    uint16_t m_, k_, n_;
    size_t aSize_, bSize_, cSize_;
};

} // namespace rb_t09

#endif
