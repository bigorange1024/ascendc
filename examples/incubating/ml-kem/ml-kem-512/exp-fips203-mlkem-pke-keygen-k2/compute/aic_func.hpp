/**
 * @file aic_func.hpp
 * @brief Tag5T Stage2：AIC Cube MMAD（int8×int8→int32）与四路 mat_c 临时写回。
 *
 * 流水线位置：mmad_custom.cpp 在 AIV Stage1 完成后，由 AIC 对 S0×LUT 做四次 Process
 *（LO_EVEN / LO_ODD / HI_EVEN / HI_ODD），结果写入 workspace 四块 mat_c_tmp。
 *
 * 作用：封装官方 LoadDataWithTranspose + Mmad + Fixpipe；本探针逻辑 m=8（紧凑 HI₄+LO₄）、
 * k=256、n=128（半列），与平面 mat_c 偶/奇半对应。
 *
 * 与 golden 关系：四路临时经 AivK4PackMatCPlanar 拼成平面 mat_c 后与 golden_mat_c 对拍；
 * 本类不直接写 dst。NTT/INTT 仅差 LUT 内容，本类逻辑相同。
 *
 * 语义：poly-batch 下 S0 已含全部 4 poly 的 HI/LO；Cube 按逻辑 8 行（硬件内部对齐），不分 limbsplit。
 */
#ifndef F203_AIC_FUNC_HPP
#define F203_AIC_FUNC_HPP

#include "basic.hpp"
#include "kernel_operator.h"
#include <cstddef>
#include <cstdint>

/**
 * 编译期 max（模板工具）。
 * @param a,b  可比较值
 * @return     较大者（类型为 T）
 */
template <typename T, typename U>
__aicore__ inline static constexpr T max(T a, U b)
{
    return (a > (T)b) ? a : (T)b;
}

/** Cube 一块 16×32 int8 的元素数，用于 A2/B2 LoadData 步进 */
static constexpr uint32_t CUBE_BLOCK_SIZE = 16 * 32;

/**
 * 向上取整除法：ceil(a/mod)。
 * @param a    被除数
 * @param mod  对齐粒度（如 16）
 */
static constexpr __aicore__ inline uint16_t ceil_div(uint16_t a, uint16_t mod)
{
    return (a + mod - 1) / mod;
}

/**
 * Stage2 AicMmad：官方 LoadDataWithTranspose 路径 + F203 mat_c 列拼接写回。
 *
 * 形状约定（本探针）：
 *   - A：S0 [m=8, k=256] int8（ND，Cube 内部对齐由 LoadData 处理）
 *   - B：LUT 半块 [k=256, n=128] int8（even/odd × top/bottom 之一）
 *   - C：临时 [m=8, n=128] int32，写到 ws+MAT_C_TMP_*
 */
class AicMmad {
public:
    /**
     * @param m  左矩阵逻辑行数（本探针 8）
     * @param k  内积维（256）
     * @param n  右矩阵列数 / 半列宽（128）
     * 构造时按 16 对齐计算 A1/A2/B/C 缓冲元素数
     */
    __aicore__ inline AicMmad(uint16_t m, uint16_t k, uint16_t n) : m(m), k(k), n(n)
    {
        const uint16_t mPadded = ceil_div(m, 16) * 16;
        const uint16_t kPadded = ceil_div(k, 16) * 16;
        aSize = mPadded * k;
        bSize = kPadded * n;
        cSize = mPadded * n;
    }

    /** 分配 A1/A2/B1/B2/CO1 队列缓冲（int8 / int32） */
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
     *
     * @param dst           C 的 GM 基址（通常为某 MAT_C_TMP_*）
     * @param a             A 的 GM（S0）
     * @param b             B 的 GM（LUT_EVEN/ODD 的 TOP 或 BOTTOM）
     * @param dstColOffset  目标矩阵列偏移（int32 元素）；本探针四路均为 0
     * @param dstRowStride  行 stride（0 表示 n）
     * 前置：仅 AIC 核调用；S0 与 LUT 已由 host/AIV 准备好
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
    /**
     * GM→A1/B1：ND 布局 DataCopy 为 NZ（Cube 友好）。
     * A：nValue=m, dValue=k；B：nValue=k, dValue=n。
     */
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

    /**
     * A1→A2：按 16 行对齐块 LoadData（不转置），供 Mmad 左矩阵。
     * 循环次数 = ceil(m/16)；本探针 m=8（Cube mmadParams.m 仍 pad 到 16） 故仅 1 次。
     */
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

    /**
     * B1→B2：LoadDataWithTranspose，右矩阵按 Cube 要求转置分块。
     * 外层循环按 k 维 32 对齐切分。
     */
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

    /**
     * A2×B2 → CO1：Mmad，cmatrixInitVal=true 表示累加器清零后写满积。
     * m 参数按 16 对齐上取整。
     */
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

    /**
     * CO1→GM：Fixpipe 写出 [m,n] int32，dstStride=dstRowStride_（默认 n）。
     */
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
