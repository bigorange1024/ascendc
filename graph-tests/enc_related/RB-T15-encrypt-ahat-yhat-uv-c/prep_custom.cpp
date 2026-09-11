/**
 * @file prep_custom.cpp
 * @brief RB-T15 Launch1：prep 半桩（AIV-only）— T07/T09 语义 ρ / y‖e1‖e2 落入共享 ws。
 *
 * 本文件在流水线中的位置：Encrypt 外形双 launch 的第一刀；Host 已用 T07 公式预算
 * y_e1_e2 与 ek，本核负责设备侧落盘与 PREP 魔数。
 *
 * 背景：真 CBD/SHAKE 设备化留给后刀；本刀允许半桩。
 * 结论：blockDim=1、KERNEL_TYPE_AIV_ONLY；无 CrossCore。
 * 未采用：在本核内重算 SHAKE/CBD。
 */
#include "kernel_operator.h"
#include "tiling.h"

/** 向 TRACE / mark GM 写 uint32 魔数（标量 `__gm__` 写，兼容 CAModel）。 */
__aicore__ inline void MarkU32(GM_ADDR base, uint32_t slotOrZero, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(base);
    *(p + slotOrZero) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * Launch1 prep：把 Host 预算的 y‖e1‖e2 与 ek 尾 ρ 写入共享 workspace。
 * @param yIn   [in]  Host 预算的 y_e1_e2[9·256] int32
 * @param ekIn  [in]  ek_PKE[1568]；本核取尾 32B 为 ρ
 * @param ws    [in/out] 共享 workspace（写 OFF_Y_E1_E2 / OFF_RHO / OFF_PREP_MARK / TRACE）
 * @param tiling 占位
 * 前置：blockDim=1；AIV-only。
 */
extern "C" __global__ __aicore__ void prep_custom(GM_ADDR yIn, GM_ADDR ekIn, GM_ADDR ws,
                                                  TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    using namespace tiling;

    AscendC::GlobalTensor<int32_t> gmYIn;
    AscendC::GlobalTensor<int32_t> gmYOut;
    AscendC::GlobalTensor<uint8_t> gmEk;
    AscendC::GlobalTensor<uint8_t> gmRho;

    gmYIn.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(yIn),
                          static_cast<uint32_t>(9 * kPolyN));
    gmYOut.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_Y_E1_E2),
                           static_cast<uint32_t>(9 * kPolyN));
    gmEk.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ekIn),
                         static_cast<uint32_t>(kEkBytes));
    gmRho.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_RHO),
                          static_cast<uint32_t>(kRhoBytes));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    constexpr uint32_t kPolyB = static_cast<uint32_t>(kPolyN) * sizeof(int32_t);
    pipe.InitBuffer(queIn, 1, kPolyB);
    pipe.InitBuffer(queOut, 1, kPolyB);

    AscendC::LocalTensor<int32_t> loc = queIn.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> locOut = queOut.AllocTensor<int32_t>();

    // ---- 逐 poly 拷贝 y(4)+e1(4)+e2(1) → ws ----
    for (int32_t p = 0; p < 9; ++p) {
        AscendC::DataCopy(loc, gmYIn[static_cast<uint32_t>(p * kPolyN)],
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t i = 0; i < kPolyN; ++i) {
            locOut.SetValue(static_cast<uint32_t>(i), loc.GetValue(static_cast<uint32_t>(i)));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmYOut[static_cast<uint32_t>(p * kPolyN)], locOut,
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    // ---- ρ ← ek 尾 32B ----
    for (uint32_t i = 0; i < static_cast<uint32_t>(kRhoBytes); ++i) {
        const uint8_t b = gmEk.GetValue(static_cast<uint32_t>(kEkBytes - kRhoBytes) + i);
        gmRho.SetValue(i, b);
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    MarkU32(ws + OFF_PREP_MARK, 0, MAGIC_PREP_MARK);
    MarkU32(ws + OFF_TRACE, SLOT_PREP_DONE, MAGIC_PREP_DONE);

    queIn.FreeTensor(loc);
    queOut.FreeTensor(locOut);
}
