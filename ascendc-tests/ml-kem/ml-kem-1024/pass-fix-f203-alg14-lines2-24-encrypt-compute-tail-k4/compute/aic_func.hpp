/**
 * @file aic_func.hpp
 * @brief Stage2 AIC：int8×int8 MMAD → int32，供 Alg.14 Encrypt NTT/INTT 的 Cube 段复用。
 *
 * 流水线位置：
 *   AIV Stage1 写 S0 [mRows,256] int8 → 本类四次 Process（偶/奇 × lo/hi LUT）
 *   → 写 MAT_C_TMP_* → AIV Pack/RouteA。
 *
 * 覆盖行号：间接服务行 16–17（NTT y）、行 19/21（INTT û/tr̂）；本文件无独立 golden。
 *
 * 几何：m×k×n 由调用方传入；Encrypt 探针常用 m=mRowsLogic（NTT 时 8 / INTT pad 时 16）、
 * k=256、n=halfN=128。dstColOffset / dstRowStride 支持列拼接写回。
 */
#ifndef F203_AIC_FUNC_HPP
#define F203_AIC_FUNC_HPP

#include "basic.hpp"
#include "kernel_operator.h"
#include <cstddef>
#include <cstdint>

/**
 * 编译期 max（模板），用于尺寸上取整。
 * @param a,b 可比较数值；返回较大者（类型为 T）。
 */
template <typename T, typename U>
__aicore__ inline static constexpr T max(T a, U b)
{
    return (a > (T)b) ? a : (T)b;
}

/** Cube 一块：16×32 int8 元素，LoadData / LoadDataWithTranspose 步进基准 */
static constexpr uint32_t CUBE_BLOCK_SIZE = 16 * 32;

/**
 * 向上取整除法：ceil(a/mod)。
 * @param a 被除数；@param mod 除数（须 >0）。
 */
static constexpr __aicore__ inline uint16_t ceil_div(uint16_t a, uint16_t mod)
{
    return (a + mod - 1) / mod;
}

/**
 * Stage2 AicMmad：官方 LoadDataWithTranspose + F203 mat_c 列拼接写回。
 *
 * 作用：GM 上 A[m,k] int8 × B[k,n] int8 → C[m,n] int32（ND 经 NZ/Cube 再 Fixpipe）。
 * 前置：调用方已保证 A/B 布局与 LUT 切片对齐；Init() 须在 Process 前调用一次。
 */
class AicMmad {
public:
    /**
     * @param m 逻辑行数（S0 行）；@param k 内积维（通常 256）；@param n 列数（通常 halfN=128）。
     * 内部按 16 对齐分配 A1/A2/B1/B2/CO1 队列容量。
     */
    __aicore__ inline AicMmad(uint16_t m, uint16_t k, uint16_t n) : m(m), k(k), n(n)
    {
        const uint16_t mPadded = ceil_div(m, 16) * 16;
        const uint16_t kPadded = ceil_div(k, 16) * 16;
        aSize = mPadded * k;
        bSize = kPadded * n;
        cSize = mPadded * n;
    }

    /** 分配 A1/A2/B1/B2/CO1 TQue；须在 Process 前调用 */
    __aicore__ inline void Init()
    {
        pipe.InitBuffer(inQueueA1, 1, aSize * sizeof(int8_t));
        pipe.InitBuffer(inQueueA2, 1, aSize * sizeof(int8_t));
        pipe.InitBuffer(inQueueB1, 1, bSize * sizeof(int8_t));
        pipe.InitBuffer(inQueueB2, 1, bSize * sizeof(int8_t));
        pipe.InitBuffer(outQueueCO1, 1, cSize * sizeof(int32_t));
    }

    /**
     * 一次完整 MMAD：CopyIn → SplitA/B → Compute → CopyOut。
     * @param dst 结果 GM 基址（int32）；实际写起点 = dst + dstColOffset
     * @param a 左矩阵 GM（S0 int8）
     * @param b 右 LUT GM（int8）
     * @param dstColOffset 目标列偏移（int32 元素）
     * @param dstRowStride 行 stride（0 表示用 n）
     */
    template <int debug_val = 0>
    __aicore__ inline void Process(GM_ADDR dst, GM_ADDR a, GM_ADDR b, uint32_t dstColOffset = 0,
                                   uint32_t dstRowStride = 0)
    {
        dstColOffset_ = dstColOffset;
        dstRowStride_ = (dstRowStride == 0) ? static_cast<uint32_t>(n) : dstRowStride;
        aGM.SetGlobalBuffer((__gm__ int8_t *)a);
        bGM.SetGlobalBuffer((__gm__ int8_t *)b);
        cGM.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(dst) + dstColOffset_);
        CopyIn<debug_val>();
        SplitA<debug_val>();
        SplitB<debug_val>();
        Compute<debug_val>();
        CopyOut<debug_val>();
    }

private:
    /** GM→A1/B1：ND→NZ DataCopy，供后续 LoadData 进 L0 */
    template <int debug_val = 0>
    __aicore__ inline void CopyIn()
    {
        AscendC::LocalTensor<int8_t> a1Local = inQueueA1.AllocTensor<int8_t>();
        AscendC::LocalTensor<int8_t> b1Local = inQueueB1.AllocTensor<int8_t>();

        // 左矩阵 A：m×k ND → NZ
        AscendC::Nd2NzParams nd2nzA1Params;
        nd2nzA1Params.ndNum = 1;
        nd2nzA1Params.nValue = m;
        nd2nzA1Params.dValue = k;
        nd2nzA1Params.srcNdMatrixStride = 0;
        nd2nzA1Params.srcDValue = k;
        nd2nzA1Params.dstNzC0Stride = ceil_div(m, 16) * 16;
        nd2nzA1Params.dstNzNStride = 1;
        nd2nzA1Params.dstNzMatrixStride = 0;
        AscendC::DataCopy(a1Local, aGM, nd2nzA1Params);

        // 右矩阵 B：k×n ND → NZ
        AscendC::Nd2NzParams nd2nzB1Params;
        nd2nzB1Params.ndNum = 1;
        nd2nzB1Params.nValue = k;
        nd2nzB1Params.dValue = n;
        nd2nzB1Params.srcNdMatrixStride = 0;
        nd2nzB1Params.srcDValue = n;
        nd2nzB1Params.dstNzC0Stride = ceil_div(k, 16) * 16;
        nd2nzB1Params.dstNzNStride = 1;
        nd2nzB1Params.dstNzMatrixStride = 0;
        AscendC::DataCopy(b1Local, bGM, nd2nzB1Params);

        inQueueA1.EnQue(a1Local);
        inQueueB1.EnQue(b1Local);
    }

    /** A1→A2：LoadData 按 16 行块装入 L0A（不转置） */
    template <int debug_val = 0>
    __aicore__ inline void SplitA()
    {
        LocalTensor<int8_t> a1Local = inQueueA1.DeQue<int8_t>();
        LocalTensor<int8_t> a2Local = inQueueA2.AllocTensor<int8_t>();

        uint32_t dstOffset = ceil_div(k, 32) * CUBE_BLOCK_SIZE;
        uint32_t srcOffset = CUBE_BLOCK_SIZE;

        AscendC::LoadData2dParams loadDataParams;
        loadDataParams.repeatTimes = ceil_div(k, 32);
        loadDataParams.srcStride = ceil_div(m, 16);
        loadDataParams.dstGap = 0;
        loadDataParams.ifTranspose = false;
        for (int32_t i = 0; i < ceil_div(m, 16); i++) {
            AscendC::LoadData(a2Local[i * dstOffset], a1Local[i * srcOffset], loadDataParams);
        }
        inQueueA1.FreeTensor(a1Local);
        inQueueA2.EnQue<int8_t>(a2Local);
    }

    /** B1→B2：LoadDataWithTranspose 装入 L0B（官方转置路径） */
    template <int debug_val = 0>
    __aicore__ inline void SplitB()
    {
        LocalTensor<int8_t> b1Local = inQueueB1.DeQue<int8_t>();
        LocalTensor<int8_t> b2Local = inQueueB2.AllocTensor<int8_t>();

        uint32_t dstOffset = ceil_div(n, 32) * (2 * CUBE_BLOCK_SIZE);
        uint32_t srcOffset = 2 * CUBE_BLOCK_SIZE;

        AscendC::LoadData2dTransposeParams loadDataParams;
        loadDataParams.repeatTimes = ceil_div(n, 32);
        loadDataParams.srcStride = ceil_div(k, 32);
        loadDataParams.dstGap = 1;
        loadDataParams.dstFracGap = 0;

        for (int i = 0; i < ceil_div(k, 32); i++) {
            AscendC::LoadDataWithTranspose(b2Local[i * dstOffset], b1Local[i * srcOffset], loadDataParams);
        }
        inQueueB1.FreeTensor(b1Local);
        inQueueB2.EnQue<int8_t>(b2Local);
    }

    /** L0A×L0B → CO1：Mmad，cmatrixInitVal=true 清零累加器 */
    template <int debug_val = 0>
    __aicore__ inline void Compute()
    {
        AscendC::LocalTensor<int8_t> a2Local = inQueueA2.DeQue<int8_t>();
        AscendC::LocalTensor<int8_t> b2Local = inQueueB2.DeQue<int8_t>();
        AscendC::LocalTensor<int32_t> c1Local = outQueueCO1.AllocTensor<int32_t>();
        AscendC::MmadParams mmadParams;
        mmadParams.m = ceil_div(m, 16) * 16;
        mmadParams.k = k;
        mmadParams.n = n;
        mmadParams.cmatrixInitVal = true;
        AscendC::Mmad(c1Local, a2Local, b2Local, mmadParams);
        outQueueCO1.EnQue<int32_t>(c1Local);
        inQueueA2.FreeTensor(a2Local);
        inQueueB2.FreeTensor(b2Local);
    }

    /** CO1→GM：Fixpipe ND 写回，dstStride=dstRowStride_ 支持列拼接 */
    template <int debug_val = 0>
    __aicore__ inline void CopyOut()
    {
        AscendC::LocalTensor<int32_t> c1Local = outQueueCO1.DeQue<int32_t>();
        AscendC::FixpipeParamsV220 fixpipeParams;
        fixpipeParams.nSize = n;
        fixpipeParams.mSize = m;
        fixpipeParams.srcStride = ceil_div(m, 16) * 16;
        fixpipeParams.dstStride = dstRowStride_;
        fixpipeParams.ndNum = 1;
        fixpipeParams.srcNdStride = 0;
        fixpipeParams.dstNdStride = 0;

        AscendC::Fixpipe(cGM, c1Local, fixpipeParams);
        outQueueCO1.FreeTensor(c1Local);
    }

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::A1, 1> inQueueA1;
    AscendC::TQue<AscendC::TPosition::A2, 1> inQueueA2;
    AscendC::TQue<AscendC::TPosition::B1, 1> inQueueB1;
    AscendC::TQue<AscendC::TPosition::B2, 1> inQueueB2;
    AscendC::TQue<AscendC::TPosition::CO1, 1> outQueueCO1;

    AscendC::GlobalTensor<int8_t> aGM;
    AscendC::GlobalTensor<int8_t> bGM;
    AscendC::GlobalTensor<int32_t> cGM;
    uint16_t m, k, n;
    size_t aSize, bSize, cSize;
    uint32_t dstColOffset_ = 0;
    uint32_t dstRowStride_ = 0;
};

#endif
