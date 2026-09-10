/**
 * @file light_cube.hpp
 * @brief RB-T30 自写极轻 Cube：单趟 16×32×32 int8→int32 MMAD（CrossCore 握手段有界真算）。
 *
 * 作用：Decrypt / Encaps 融合核 AIC 路径各段调用；与 ML-KEM 系数数学无关。
 * API 用法对齐工程笔记（Nd2Nz / LoadData / Mmad / Fixpipe）；整文件自写，非抄他案。
 */
#ifndef RB_T30_LIGHT_CUBE_HPP
#define RB_T30_LIGHT_CUBE_HPP

#include "kernel_operator.h"
#include "tiling.h"
#include <cstdint>

namespace rb_t30 {

static constexpr uint32_t kCubeFrac = 16 * 32;

static constexpr __aicore__ inline uint16_t DivCeilU16(uint16_t a, uint16_t mod)
{
    return static_cast<uint16_t>((a + mod - 1) / mod);
}

/**
 * 极轻握手 matmul：C[16,32] = A[16,32] × B[32,32]（int8→int32）。
 */
class T30LightCube {
public:
    __aicore__ inline T30LightCube()
    {
        // 默认按 Decrypt 侧 mat 形状；Encaps 同形（16/32/32）
        m_ = t30_dec::kM;
        k_ = t30_dec::kKk;
        n_ = t30_dec::kN;
        aBytes_ = static_cast<size_t>(m_) * k_;
        bBytes_ = static_cast<size_t>(k_) * n_;
        cElems_ = static_cast<size_t>(m_) * n_;
    }

    /** 分配 L1/L0/CO1 队列缓冲。 */
    __aicore__ inline void Init()
    {
        pipe_.InitBuffer(qA1_, 1, aBytes_ * sizeof(int8_t));
        pipe_.InitBuffer(qA2_, 1, aBytes_ * sizeof(int8_t));
        pipe_.InitBuffer(qB1_, 1, bBytes_ * sizeof(int8_t));
        pipe_.InitBuffer(qB2_, 1, bBytes_ * sizeof(int8_t));
        pipe_.InitBuffer(qCo1_, 1, cElems_ * sizeof(int32_t));
    }

    /**
     * 执行一次 Cube 握手计算。
     * @param dstGm C 输出（int32 ND）
     * @param aGm   A 输入（int8 ND）
     * @param bGm   B 输入（int8 ND）
     */
    __aicore__ inline void RunOnce(GM_ADDR dstGm, GM_ADDR aGm, GM_ADDR bGm)
    {
        aGM_.SetGlobalBuffer(reinterpret_cast<__gm__ int8_t *>(aGm));
        bGM_.SetGlobalBuffer(reinterpret_cast<__gm__ int8_t *>(bGm));
        cGM_.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(dstGm));
        FetchNd();
        LiftA();
        LiftB();
        MmadCore();
        StoreNd();
    }

private:
    /** GM ND → L1（A1/B1）via Nd2Nz。 */
    __aicore__ inline void FetchNd()
    {
        AscendC::LocalTensor<int8_t> a1 = qA1_.AllocTensor<int8_t>();
        AscendC::LocalTensor<int8_t> b1 = qB1_.AllocTensor<int8_t>();

        AscendC::Nd2NzParams pa;
        pa.ndNum = 1;
        pa.nValue = m_;
        pa.dValue = k_;
        pa.srcNdMatrixStride = 0;
        pa.srcDValue = k_;
        pa.dstNzC0Stride = DivCeilU16(m_, 16) * 16;
        pa.dstNzNStride = 1;
        pa.dstNzMatrixStride = 0;
        AscendC::DataCopy(a1, aGM_, pa);

        AscendC::Nd2NzParams pb;
        pb.ndNum = 1;
        pb.nValue = k_;
        pb.dValue = n_;
        pb.srcNdMatrixStride = 0;
        pb.srcDValue = n_;
        pb.dstNzC0Stride = DivCeilU16(k_, 16) * 16;
        pb.dstNzNStride = 1;
        pb.dstNzMatrixStride = 0;
        AscendC::DataCopy(b1, bGM_, pb);

        qA1_.EnQue(a1);
        qB1_.EnQue(b1);
    }

    /** A1 → A2（LoadData）。 */
    __aicore__ inline void LiftA()
    {
        AscendC::LocalTensor<int8_t> a1 = qA1_.DeQue<int8_t>();
        AscendC::LocalTensor<int8_t> a2 = qA2_.AllocTensor<int8_t>();
        uint32_t dstOff = DivCeilU16(k_, 32) * kCubeFrac;
        uint32_t srcOff = kCubeFrac;
        AscendC::LoadData2dParams lp;
        lp.repeatTimes = DivCeilU16(k_, 32);
        lp.srcStride = DivCeilU16(m_, 16);
        lp.dstGap = 0;
        lp.ifTranspose = false;
        for (int32_t i = 0; i < DivCeilU16(m_, 16); ++i) {
            AscendC::LoadData(a2[i * dstOff], a1[i * srcOff], lp);
        }
        qA1_.FreeTensor(a1);
        qA2_.EnQue<int8_t>(a2);
    }

    /** B1 → B2（LoadDataWithTranspose）。 */
    __aicore__ inline void LiftB()
    {
        AscendC::LocalTensor<int8_t> b1 = qB1_.DeQue<int8_t>();
        AscendC::LocalTensor<int8_t> b2 = qB2_.AllocTensor<int8_t>();
        uint32_t dstOff = DivCeilU16(n_, 32) * (2 * kCubeFrac);
        uint32_t srcOff = 2 * kCubeFrac;
        AscendC::LoadData2dTransposeParams lp;
        lp.repeatTimes = DivCeilU16(n_, 32);
        lp.srcStride = DivCeilU16(k_, 32);
        lp.dstGap = 1;
        lp.dstFracGap = 0;
        for (int32_t i = 0; i < DivCeilU16(k_, 32); ++i) {
            AscendC::LoadDataWithTranspose(b2[i * dstOff], b1[i * srcOff], lp);
        }
        qB1_.FreeTensor(b1);
        qB2_.EnQue<int8_t>(b2);
    }

    /** L0A×L0B → CO1。 */
    __aicore__ inline void MmadCore()
    {
        AscendC::LocalTensor<int8_t> a2 = qA2_.DeQue<int8_t>();
        AscendC::LocalTensor<int8_t> b2 = qB2_.DeQue<int8_t>();
        AscendC::LocalTensor<int32_t> c1 = qCo1_.AllocTensor<int32_t>();
        AscendC::MmadParams mp;
        mp.m = DivCeilU16(m_, 16) * 16;
        mp.k = k_;
        mp.n = n_;
        mp.cmatrixInitVal = true;
        AscendC::Mmad(c1, a2, b2, mp);
        qCo1_.EnQue<int32_t>(c1);
        qA2_.FreeTensor(a2);
        qB2_.FreeTensor(b2);
    }

    /** CO1 → GM ND（Fixpipe）。 */
    __aicore__ inline void StoreNd()
    {
        AscendC::LocalTensor<int32_t> c1 = qCo1_.DeQue<int32_t>();
        AscendC::FixpipeParamsV220 fp;
        fp.nSize = n_;
        fp.mSize = m_;
        fp.srcStride = DivCeilU16(m_, 16) * 16;
        fp.dstStride = n_;
        fp.ndNum = 1;
        fp.srcNdStride = 0;
        fp.dstNdStride = 0;
        AscendC::Fixpipe(cGM_, c1, fp);
        qCo1_.FreeTensor(c1);
    }

    AscendC::TPipe pipe_;
    AscendC::TQue<AscendC::TPosition::A1, 1> qA1_;
    AscendC::TQue<AscendC::TPosition::A2, 1> qA2_;
    AscendC::TQue<AscendC::TPosition::B1, 1> qB1_;
    AscendC::TQue<AscendC::TPosition::B2, 1> qB2_;
    AscendC::TQue<AscendC::TPosition::CO1, 1> qCo1_;
    AscendC::GlobalTensor<int8_t> aGM_;
    AscendC::GlobalTensor<int8_t> bGM_;
    AscendC::GlobalTensor<int32_t> cGM_;
    uint16_t m_, k_, n_;
    size_t aBytes_, bBytes_, cElems_;
};

} // namespace rb_t30

#endif
