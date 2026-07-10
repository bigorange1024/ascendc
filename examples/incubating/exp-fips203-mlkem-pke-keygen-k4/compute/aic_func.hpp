// @probe exp-fips203-mlkem-pke-keygen-k4
// @file compute/aic_func.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `aic_func.hpp` 为该子模块组件。 / Component: aic_func.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: basic.hpp, kernel_operator.h, cstddef, cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file aic_func.hpp
 * @brief KeyGen Launch 2 — AIC 侧 Stage2 Cube MMAD（int8×int8→int32）。
 *
 * ## 流水线位置
 * Tag5T NTT Stage2：左矩阵 S0（limb6 int8）× 右 LUT → mat_c 临时竖堆。
 * 由 `mmad_custom` 在 AIC 上调用四次（even/odd × lo/hi）。
 *
 * ## 对齐与 golden
 * FIPS 203 Alg.13 / ML-KEM-1024（k=4）；与 Host golden 仅 I/O 等价，不要求实现同构。
 */
#ifndef F203_AIC_FUNC_HPP
#define F203_AIC_FUNC_HPP

#include "basic.hpp"
#include "kernel_operator.h"
#include <cstddef>
#include <cstdint>

/** 编译期 max，供 pad 计算 */
template <typename T, typename U>
__aicore__ inline static constexpr T max(T a, U b)
{
    return (a > (T)b) ? a : (T)b;
}

/** Cube 分块：16×32 int8 元素为一块（LoadData 步进） */
static constexpr uint32_t CUBE_BLOCK_SIZE = 16 * 32;

/** 向上取整除法：a 按 mod 对齐所需块数 */
static constexpr __aicore__ inline uint16_t ceil_div(uint16_t a, uint16_t mod)
{
    return (a + mod - 1) / mod;
}

/**
 * Stage2 AicMmad：官方 LoadDataWithTranspose 路径 + Fixpipe 写回。
 * 几何：A[m,k] int8 × B[k,n] int8 → C[m,n] int32（m/k 按 16 对齐 pad）。
 */
class AicMmad {
public:
    /**
     * @param m 左矩阵行（逻辑 mRowsLogic，如 32）
     * @param k 归约维（coeffN=256）
     * @param n 右矩阵列（halfN=128，偶/奇半列）
     */
    __aicore__ inline AicMmad(uint16_t m, uint16_t k, uint16_t n) : m(m), k(k), n(n)
    {
        const uint16_t mPadded = ceil_div(m, 16) * 16;
        const uint16_t kPadded = ceil_div(k, 16) * 16;
        aSize = mPadded * k;
        bSize = kPadded * n;
        cSize = mPadded * n;
    }

    /** 初始化 A1/A2/B1/B2/CO1 五级 TQue 缓冲 */
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
     * @param dst          结果 GM（int32），可带列偏移
     * @param a            左矩阵 GM int8（S0）
     * @param b            右 LUT GM int8
     * @param dstColOffset 目标矩阵列偏移（int32 元素）
     * @param dstRowStride 行 stride（0 表示 n）
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
    /** GM ND → A1/B1 NZ：左 [m,k]、右 [k,n] */
    template <int debug_val = 0>
    __aicore__ inline void CopyIn()
    {
        AscendC::LocalTensor<int8_t> a1Local = inQueueA1.AllocTensor<int8_t>();
        AscendC::LocalTensor<int8_t> b1Local = inQueueB1.AllocTensor<int8_t>();

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

    /** A1→A2：按 16 行块 LoadData 到 L0A 布局 */
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
        // i：按 16 行一块遍历 m 维
        for (int32_t i = 0; i < ceil_div(m, 16); i++) {
            AscendC::LoadData(a2Local[i * dstOffset], a1Local[i * srcOffset], loadDataParams);
        }
        inQueueA1.FreeTensor(a1Local);
        inQueueA2.EnQue<int8_t>(a2Local);
    }

    /** B1→B2：LoadDataWithTranspose，右矩阵进 L0B */
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

        // i：按 k 维 32 对齐块遍历
        for (int i = 0; i < ceil_div(k, 32); i++) {
            AscendC::LoadDataWithTranspose(b2Local[i * dstOffset], b1Local[i * srcOffset], loadDataParams);
        }
        inQueueB1.FreeTensor(b1Local);
        inQueueB2.EnQue<int8_t>(b2Local);
    }

    /** Cube Mmad：cmatrixInitVal=true 表示累加器清零后一次乘加 */
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

    /** CO1 → GM：Fixpipe，dstStride 支持列拼接写回 */
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
