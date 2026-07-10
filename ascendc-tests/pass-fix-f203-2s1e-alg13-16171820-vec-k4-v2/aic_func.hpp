/**
 * @file aic_func.hpp
 * @brief Stage2 AIC Cube：int8 MMAD（S0 × LUT → mat_c 临时半矩阵）。
 *
 * 流水线位置：mmad_custom 在 AIV Stage1 完成后调用 AicMmad×4（even/odd × lo/hi）。
 * 作用：官方 LoadDataWithTranspose 路径；输出 int32 半宽列（halfN=128）写入 ws 临时区。
 * 与 golden 关系：中间态；最终对拍经平面 pack + Stage3 后的 dst.bin，非单独 MMAD golden
 * （mixPass=6 除外，见 gen_mmad_sanity_data.py）。
 */
#ifndef F203_AIC_FUNC_HPP
#define F203_AIC_FUNC_HPP

#include "basic.hpp"
#include "kernel_operator.h"
#include <cstddef>
#include <cstdint>

/** 编译期 max（模板工具）。 */
template <typename T, typename U>
__aicore__ inline static constexpr T max(T a, U b)
{
    return (a > (T)b) ? a : (T)b;
}

/** Cube 分块：16×32 int8 元素块大小（LoadData 步进）。 */
static constexpr uint32_t CUBE_BLOCK_SIZE = 16 * 32;

/** 向上取整除法：a 按 mod 对齐所需块数。 */
static constexpr __aicore__ inline uint16_t ceil_div(uint16_t a, uint16_t mod)
{
    return (a + mod - 1) / mod;
}

/**
 * Stage2 AicMmad：A[m,k] int8 × B[k,n] int8 → C[m,n] int32。
 * 本探针典型：m=mRowsLogic(32)、k=256、n=halfN(128)；sanity 用 m=16。
 */
class AicMmad {
public:
    /**
     * @param m 左矩阵行（S0 有效行）；@param k 收缩维（系数 n=256）；@param n 右矩阵列（半宽 128）
     * 内部按 16 对齐计算 A1/A2/B/CO1 缓冲字节数。
     */
    __aicore__ inline AicMmad(uint16_t m, uint16_t k, uint16_t n) : m(m), k(k), n(n)
    {
        const uint16_t mPadded = ceil_div(m, 16) * 16;
        const uint16_t kPadded = ceil_div(k, 16) * 16;
        aSize = mPadded * k;
        bSize = kPadded * n;
        cSize = mPadded * n;
    }

    /** 分配 A1/A2/B1/B2/CO1 TQue 缓冲（单次 Process 复用）。 */
    __aicore__ inline void Init()
    {
        pipe.InitBuffer(inQueueA1, 1, aSize * sizeof(int8_t));
        pipe.InitBuffer(inQueueA2, 1, aSize * sizeof(int8_t));
        pipe.InitBuffer(inQueueB1, 1, bSize * sizeof(int8_t));
        pipe.InitBuffer(inQueueB2, 1, bSize * sizeof(int8_t));
        pipe.InitBuffer(outQueueCO1, 1, cSize * sizeof(int32_t));
    }

    /**
     * 执行一次 MMAD：GM a×b → GM dst。
     * @param dst 结果基址（int32）；@param a S0 int8；@param b LUT 半片 int8
     * @param dstColOffset 目标列偏移（int32 元素）；@param dstRowStride 行 stride（0→n）
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
    /** GM ND → A1/B1 NZ：左矩阵 [m,k]、右矩阵 [k,n] int8。 */
    template <int debug_val = 0>
    __aicore__ inline void CopyIn()
    {
        AscendC::LocalTensor<int8_t> a1Local = inQueueA1.AllocTensor<int8_t>();
        AscendC::LocalTensor<int8_t> b1Local = inQueueB1.AllocTensor<int8_t>();

        /* 左矩阵 A：行 m、列 k，ND→NZ */
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

        /* 右矩阵 B：行 k、列 n（半宽 LUT） */
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

    /** A1→A2：按 16 行块 LoadData（不转置）。 */
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

    /** B1→B2：LoadDataWithTranspose，供 Cube 右矩阵格式。 */
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

    /** Cube Mmad：cmatrixInitVal=true 清零累加器后做 int8×int8→int32。 */
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

    /** CO1→GM：Fixpipe 按 dstRowStride_ 写回 ND int32。 */
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
