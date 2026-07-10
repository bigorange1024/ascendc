/**
 * @file aic_func.hpp
 * @brief Stage2：AIC（Cube 核）上执行的 int8 矩阵乘 `C[int32] = A[int8] @ B[int8]`
 * 封装类 `AicMmad`，是本探针（pass-toy-mix-s123-byteencode-k2）MIX 三阶段流水
 * 中 S2（Cube matmul）的唯一实现来源，被 `mmad_custom.cpp` 在 AIC 分支下
 * 构造调用：`AicMmad mmad(64,64,64); mmad.Init(); mmad.Process(ws+MAT_C, ws+S0, ws+LUT)`。
 *
 * 与 golden 的关系：本类只负责把左矩阵 A（Stage1 写入 ws+S0）与右矩阵 B（host
 * 预填的单位阵 I₆₄，位于 ws+LUT）相乘，结果写入 ws+MAT_C；由于 B=I，C 数值上
 * 应等于 A 的 int32 扩宽版本，供 Stage3 消费。真实 NTT/KeyGen 场景中对应
 * `AicMmad(16,256,128)`（形状不同但流程一致），此处用最小 64³ 方阵验证
 * Cube 侧 Nd2Nz 搬入 → LoadData2d 分块重排 → Mmad 累乘 → Fixpipe 搬出 的
 * 标准四段流水（ND→NZ→计算→输出）。
 *
 * Cube 编程模型要点（AI Core Cube 单元要求特定内存排布 NZ，与常规行优先 ND
 * 不同）：
 *   1. CopyIn：GM 上的 ND（普通行优先）矩阵通过 `DataCopy`+`Nd2NzParams`
 *      搬入 A1/B1（NZ 分块转换在搬运时完成）；
 *   2. SplitA/SplitB：A1/B1 → A2/B2，用 `LoadData`/`LoadDataWithTranspose`
 *      把数据重排为 Cube 硬件要求的 16×32（int8）子块交织格式（A 不转置，
 *      B 需转置以匹配 Mmad 对右矩阵的排布要求）；
 *   3. Compute：`AscendC::Mmad` 执行 A2 @ B2 → C1（int32 累加，NZ 排布）；
 *   4. CopyOut：`AscendC::Fixpipe` 把 NZ 排布的 C1 转换回 ND 排布写回 GM。
 */
#ifndef __AIC_FUNC_HPP__
#define __AIC_FUNC_HPP__
#include "kernel_operator.h"
#include "basic.hpp"
#include <cstddef>
#include <cstdint>

/** 通用二元最大值（T/U 类型可不同，统一转换到 T 比较），用于下方尺寸计算中的下限保护。 */
template <typename T, typename U> 
__aicore__ inline static constexpr T max(T a, U b) {return (a > (T)b) ? a : (T)b;}

/* Cube 硬件对 int8_t 矩阵要求的最小分块单元：16 行 × 32 列（Cube Z 排布的基本粒度），
 * 用于 SplitA/SplitB 中计算源/目的偏移跨距。 */
// int8_t type, cube block: [16, 32]
static constexpr uint32_t CUBE_BLOCK_SIZE = 16 * 32;
/** 保留常量：256×256 int32 矩阵字节数，当前文件未直接使用（历史遗留，供潜在更大 tiling 参考）。 */
constexpr size_t matMFileSize = 256 * 256 * sizeof(int32_t);
/** 向上取整除法：ceil(a/mod)，用于把任意 m/k/n 维度对齐到 Cube 分块粒度（16 或 32）。 */
static constexpr __aicore__ inline uint16_t ceil_div(uint16_t a, uint16_t mod) {
    return (a + mod - 1) / mod;
} 

/**
 * @class AicMmad
 * @brief Cube 侧 int8 矩阵乘封装：`C[m,n] int32 = A[m,k] int8 @ B[k,n] int8`。
 *
 * 使用方式（本探针内固定 m=k=n=64，对应 tiling::kRows/kDim/kCols）：
 *   AicMmad mmad(m, k, n);
 *   mmad.Init();                       // 申请各级 TQue（A1/A2/B1/B2/CO1）UB/L0 缓冲
 *   mmad.Process(dstGM, aGM, bGM);      // CopyIn → SplitA → SplitB → Compute → CopyOut
 *
 * 前置条件：k、n 需为 32 的倍数（Cube int8 分块粒度约束，本探针 k=n=64 满足）；
 * m 允许 < 16（内部用 max(16,m) 兜底到 Cube 最小行分块）。
 */
class AicMmad {
public:
    /**
     * 构造：记录矩阵维度 m/k/n，并据此计算 A1/A2、B1/B2、CO1 缓冲区所需元素个数。
     * @param m [in] 左矩阵 A 的行数 / 输出 C 的行数
     * @param k [in] 左矩阵 A 的列数 / 右矩阵 B 的行数（内积维度，需为 32 的倍数）
     * @param n [in] 右矩阵 B 的列数 / 输出 C 的列数（需为 32 的倍数）
     * 前置条件：m/k/n 均以 tiling.h 中的 kRows/kDim/kCols（64/64/64）传入。
     */
    __aicore__ inline AicMmad(uint16_t m, uint16_t k, uint16_t n) :
        m(m), k(k), n(n)
    {
        // k, n 需要是 32 的倍数
        /* aSize：A 矩阵占用元素数，行数按 Cube 最小分块 16 向上取整（max(16,m)）后乘 k；
         * bSize：B 矩阵占用元素数 = k*n（B 本身按行×列铺满，无需 16 对齐，因其转置搬运时
         * 已按 32 对齐处理，见 SplitB）；
         * cSize：输出 C 占用元素数，同 A 按 16 对齐的行数乘 n。 */
        aSize = max(16, m) * k;
        bSize = k * n;
        cSize = max(16, m) * n;
        // aSize = max(aSize, 1056);
        // bSize = max(bSize, 1056);
        // cSize = max(cSize, 1056);
    }
    /**
     * 初始化各级流水缓冲（TQue）：A1/B1（ND→NZ 搬入后的中间态）、A2/B2（Cube 分块
     * 重排后的计算态）、CO1（Cube 输出）。TPosition 分别对应 Cube 硬件的 L1(A1/B1)、
     * L0A/L0B(A2/B2)、L0C(CO1) 存储层级（由 AscendC::TPosition 枚举语义决定）。
     */
    __aicore__ inline void Init()
    {
        // KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AICORE);
        pipe.InitBuffer(inQueueA1, 1, aSize * sizeof(int8_t));
        pipe.InitBuffer(inQueueA2, 1, aSize * sizeof(int8_t));
        pipe.InitBuffer(inQueueB1, 1, bSize * sizeof(int8_t));
        pipe.InitBuffer(inQueueB2, 1, bSize * sizeof(int8_t));
        pipe.InitBuffer(outQueueCO1, 1, cSize * sizeof(int32_t));
    }
    /**
     * 执行完整 Cube matmul 流水：CopyIn（ND→NZ 搬入）→ SplitA/SplitB（Cube 分块重排）
     * → Compute（Mmad 累乘）→ CopyOut（Fixpipe 搬出）。
     * @param dst [out] GM 指针，输出矩阵 C，形状 [m,n] int32，行优先 ND 排布
     * @param a   [in]  GM 指针，左矩阵 A，形状 [m,k] int8，行优先 ND 排布
     * @param b   [in]  GM 指针，右矩阵 B，形状 [k,n] int8，行优先 ND 排布
     * 前置条件：本函数须在 Init() 之后调用；dst/a/b 三者互不重叠。
     */
    template <int debug_val = 0>
    __aicore__ inline void Process(GM_ADDR dst, GM_ADDR a, GM_ADDR b)
    {
        aGM.SetGlobalBuffer((__gm__ int8_t *)a);
        bGM.SetGlobalBuffer((__gm__ int8_t *)b);
        cGM.SetGlobalBuffer((__gm__ int32_t *)dst);
        CopyIn<debug_val>();
        SplitA<debug_val>();
        SplitB<debug_val>();
        Compute<debug_val>();
        CopyOut<debug_val>();
    }

private:
    /**
     * Stage2 CopyIn：把 GM 上行优先 ND 排布的 A、B 矩阵，通过 `AscendC::DataCopy`
     * + `Nd2NzParams` 一次性搬入 A1/B1（L1）缓冲，搬运过程中同时完成 ND→NZ 的
     * 分块重排（Cube 硬件要求的存储格式），供后续 SplitA/SplitB 进一步整理为
     * L0A/L0B 计算态。
     *
     * Nd2NzParams 字段语义（A 矩阵为例，B 矩阵同理把 m 换成 k、k 换成 n）：
     *   ndNum=1            —— 单个矩阵（非批量 ND 矩阵搬运）
     *   nValue=m           —— 源矩阵行数
     *   dValue=k           —— 源矩阵列数（即 NZ 排布中的 D 维）
     *   srcNdMatrixStride  —— 源多矩阵间跨距，ndNum=1 时为 0（不涉及）
     *   srcDValue=k        —— 源矩阵行跨距（ND 排布下等于列数，行优先无 padding）
     *   dstNzC0Stride      —— 目的 NZ 排布中，相邻 C0（16 行一组）分块间的跨距，
     *                         按行数向上取整到 16 的倍数（Cube 硬件按 16 行分块）
     *   dstNzNStride=1     —— 目的 NZ 内部行方向连续排布
     *   dstNzMatrixStride  —— 目的多矩阵间跨距，单矩阵场景为 0
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
        nd2nzA1Params.dstNzC0Stride = ceil_div(m, 16) * 16; //?
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
     * Stage2 SplitA：把 CopyIn 得到的 A1（L1，NZ 排布但仍以 16 行为粒度线性排列）
     * 通过 `AscendC::LoadData` 重排为 A2（L0A，Cube Mmad 实际读取的分块交织格式，
     * 每块 16×32 int8，即 CUBE_BLOCK_SIZE）。A 矩阵作为左矩阵不需要转置
     * （`ifTranspose=false`）。
     *
     * 循环语义：外层 `i` 遍历 A 的行方向分块（共 ceil_div(m,16) 块，每块 16 行）；
     * 对第 i 块：dstOffset 为 A2 中该行块的起始偏移（按 32 列一组的 Cube 块跨距），
     * srcOffset 为 A1 中该行块的起始偏移（每 16 行占 CUBE_BLOCK_SIZE 元素）；
     * loadDataParams.repeatTimes = ceil_div(k,32) 表示沿列方向重复搬运多少个
     * 32 列的 Cube 分块，srcStride 为源分块间跨距（按 16 行块数）。
     *
     * CPU 调试分支：把 a2Local 整块预填为 -1，便于在仿真日志中区分「LoadData
     * 未覆盖到的区域」（若最终结果里仍出现 -1，说明分块参数算错、留有空洞）。
     */
    template <int debug_val = 0>
    __aicore__ inline void SplitA() {
        LocalTensor<int8_t> a1Local = inQueueA1.DeQue<int8_t>();
        LocalTensor<int8_t> a2Local = inQueueA2.AllocTensor<int8_t>();
        
        uint32_t dstOffset = ceil_div(k, 32) * CUBE_BLOCK_SIZE;
        uint32_t srcOffset = CUBE_BLOCK_SIZE;

        #ifdef ASCENDC_CPU_DEBUG
        for(int i = 0; i < max(16, m) * k; i++)
            a2Local.SetValue(i, -1);
        #endif

        AscendC::LoadData2dParams loadDataParams;
        loadDataParams.repeatTimes = ceil_div(k, 32);
        loadDataParams.srcStride = ceil_div(m, 16);
        loadDataParams.dstGap = 0;
        loadDataParams.ifTranspose = false;
        for (int32_t i = 0; i < ceil_div(m, 16); i++) {
            // __assertion_info("i = %d, dstOffset = %d, srcOffset = %d", i, dstOffset, srcOffset);
            AscendC::LoadData(a2Local[i * dstOffset], a1Local[i * srcOffset], loadDataParams);
        }
        // __print_tensor_short(a1Local, 1 * 64, 64);
        // __print_tensor_short(a2Local[0], 32, 32);
        // __print_tensor_short(a2Local[32 * 16], 32, 32);
        inQueueA1.FreeTensor(a1Local);
        inQueueA2.EnQue<int8_t>(a2Local);
    }

    /**
     * Stage2 SplitB：把 CopyIn 得到的 B1（L1）通过 `AscendC::LoadDataWithTranspose`
     * 重排为 B2（L0B），与 SplitA 的区别是 B 作为右矩阵**需要转置**排布
     * （Mmad 硬件对右矩阵的读取格式要求），故用 `LoadData2dTransposeParams`
     * 而非普通 `LoadData2dParams`。
     *
     * 循环语义：外层 `i` 遍历 B 的行方向（k 维）分块，共 ceil_div(k,32) 块（每块
     * 32 行，注意此处分块粒度是 32 而不是 A 的 16，因为转置操作按 2×CUBE_BLOCK_SIZE
     * 处理源跨距）；dstOffset/srcOffset 分别为目的/源在该分块下的偏移；
     * `dstGap=1` 表示目的分块间预留 1 个 fractal 的间隙（转置排布的硬件对齐要求），
     * `dstFracGap=0` 表示分块内部无额外间隙。
     */
    template <int debug_val = 0>
    __aicore__ inline void SplitB() {
        LocalTensor<int8_t> b1Local = inQueueB1.DeQue<int8_t>();
        LocalTensor<int8_t> b2Local = inQueueB2.AllocTensor<int8_t>();

        uint32_t dstOffset = ceil_div(n, 32) * (2 * CUBE_BLOCK_SIZE);
        uint32_t srcOffset = 2 * CUBE_BLOCK_SIZE;

        AscendC::LoadData2dTransposeParams loadDataParams;
        loadDataParams.repeatTimes = ceil_div(n, 32);
        loadDataParams.srcStride = ceil_div(k, 32);
        loadDataParams.dstGap = 1;
        loadDataParams.dstFracGap = 0;

        #ifdef ASCENDC_CPU_DEBUG
            for(int i = 0; i < k * n; i++)
                b2Local.SetValue(i, -1);
        #endif

        for(int i = 0; i < ceil_div(k, 32); i++) {
            AscendC::LoadDataWithTranspose(b2Local[i * dstOffset], b1Local[i * srcOffset], loadDataParams);
        }
        // __print_tensor_short(b2Local, 64 * 64, 64);
        inQueueB1.FreeTensor(b1Local);
        inQueueB2.EnQue<int8_t>(b2Local);
    }

    /**
     * Stage2 Compute：调用 `AscendC::Mmad` 对 A2（L0A）、B2（L0B）做矩阵乘累加，
     * 结果写入 C1（L0C，int32）。
     * @param mmadParams.m [in] 参与计算的行数，向上取整到 16（Cube 硬件粒度）
     * @param mmadParams.k [in] 内积维度（与构造函数 k 一致）
     * @param mmadParams.n [in] 输出列数（与构造函数 n 一致）
     * @param mmadParams.cmatrixInitVal [in] true 表示 C 矩阵先清零再累加
     *        （本探针每次单趟计算，不需要跨多次调用累加，故每次都重新初始化）
     * 本探针中 B=I₆₄（单位阵），故 C 数值上应等于 A 的 int32 扩宽版本（无信息损失）。
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
     * Stage2 CopyOut：调用 `AscendC::Fixpipe` 把 Compute 产出的 NZ 排布 C1（L0C）
     * 转换回标准行优先 ND 排布，写回 GM（cGM，即调用侧传入的 dst）。
     *
     * FixpipeParamsV220 字段语义：
     *   nSize/mSize    —— 输出矩阵的列数/行数（未做 16 对齐的真实 m/n）
     *   srcStride      —— 源 NZ 矩阵中相邻 Z 排布块的起始地址偏移，单位为
     *                     C0_Size(=16*sizeof(T))；此处按 16 对齐后的行数换算
     *   dstStride      —— 目的 ND 矩阵的行跨距（即列数 n，标准行优先无 padding）
     *   ndNum/srcNdStride/dstNdStride —— 单矩阵场景（ndNum=1）不涉及多矩阵批量跨距
     */
    template <int debug_val = 0>
    __aicore__ inline void CopyOut()
    {
        AscendC::LocalTensor<int32_t> c1Local = outQueueCO1.DeQue<int32_t>();
        // __print_tensor_short(c1Local, max(16,m) * n, 32)
        // __assertion_info("m = %d, n = %d", m, n)
        AscendC::FixpipeParamsV220 fixpipeParams;
        fixpipeParams.nSize = n;
        fixpipeParams.mSize = m;
        // 源NZ矩阵中相邻Z排布的起始地址偏移，取值范围：srcStride∈[0, 65535]， 单位：C0_Size(16*sizeof(T)，T为srcLocal的数据类型)。
        // C0_Size(16*sizeof(T)，T为srcLocal的数据类型)。
        fixpipeParams.srcStride = ceil_div(m, 16) * 16;
        fixpipeParams.dstStride = n;
        fixpipeParams.ndNum = 1;
        fixpipeParams.srcNdStride = 0;
        fixpipeParams.dstNdStride = 0;

        AscendC::Fixpipe(cGM, c1Local, fixpipeParams);
        outQueueCO1.FreeTensor(c1Local);
    }

private:
    AscendC::TPipe pipe;
    /* A1/B1：L1 缓冲，CopyIn（Nd2Nz）搬入后的中间态；A2/B2：L0A/L0B 缓冲，
     * SplitA/SplitB（LoadData）重排后的 Cube 计算态；CO1：L0C 缓冲，Compute
     * （Mmad）输出，CopyOut（Fixpipe）读出写回 GM。TPosition 决定了各 TQue
     * 底层挂载到 Cube 硬件的哪一级存储。 */
    AscendC::TQue<AscendC::TPosition::A1, 1> inQueueA1;
    AscendC::TQue<AscendC::TPosition::A2, 1> inQueueA2;
    AscendC::TQue<AscendC::TPosition::B1, 1> inQueueB1;
    AscendC::TQue<AscendC::TPosition::B2, 1> inQueueB2;
    AscendC::TQue<AscendC::TPosition::CO1, 1> outQueueCO1;

    AscendC::GlobalTensor<int8_t> aGM;   // 左矩阵 A 的 GM 视图（Process 中绑定）
    AscendC::GlobalTensor<int8_t> bGM;   // 右矩阵 B 的 GM 视图
    AscendC::GlobalTensor<int32_t> cGM;  // 输出矩阵 C 的 GM 视图
    uint16_t m, k, n;          // 矩阵维度：C[m,n] = A[m,k] @ B[k,n]
    size_t aSize, bSize, cSize;  // 对应各矩阵在 UB/L0 缓冲中占用的元素个数（含 16 对齐 padding）
};

#endif