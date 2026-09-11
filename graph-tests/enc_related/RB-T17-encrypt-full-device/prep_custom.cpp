/**
 * @file prep_custom.cpp
 * @brief RB-T17 Launch1：prep（AIV-only）— ρ←ek 尾；coins 已由 Host 写入 OFF_COINS。
 *
 * 相对 T15：不再拷贝 Host 预算的 y‖e1‖e2（改由 Launch2 设备 CBD）。
 * 结论：blockDim=1、KERNEL_TYPE_AIV_ONLY；无 CrossCore。
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
 * Launch1 prep：把 ek 尾 ρ 写入共享 workspace；确认 coins 槽已由 Host 装填。
 * @param coinsIn [in]  Host coins[32]（再镜像到 OFF_COINS，防 ws 未带齐）
 * @param ekIn    [in]  ek_PKE[1568]；本核取尾 32B 为 ρ
 * @param ws      [in/out] 写 OFF_COINS / OFF_RHO / OFF_PREP_MARK / TRACE
 * @param tiling  占位
 * 前置：blockDim=1；AIV-only。
 */
extern "C" __global__ __aicore__ void prep_custom(GM_ADDR coinsIn, GM_ADDR ekIn, GM_ADDR ws,
                                                  TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    using namespace tiling;

    AscendC::GlobalTensor<uint8_t> gmCoinsIn;
    AscendC::GlobalTensor<uint8_t> gmCoinsWs;
    AscendC::GlobalTensor<uint8_t> gmEk;
    AscendC::GlobalTensor<uint8_t> gmRho;

    gmCoinsIn.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(coinsIn),
                              static_cast<uint32_t>(kCoinsBytes));
    gmCoinsWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_COINS),
                              static_cast<uint32_t>(kCoinsBytes));
    gmEk.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ekIn),
                         static_cast<uint32_t>(kEkBytes));
    gmRho.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_RHO),
                          static_cast<uint32_t>(kRhoBytes));

    // ---- coins → ws（设备侧可见；禁最终 y/e）----
    for (uint32_t i = 0; i < static_cast<uint32_t>(kCoinsBytes); ++i) {
        gmCoinsWs.SetValue(i, gmCoinsIn.GetValue(i));
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---- ρ ← ek 尾 32B ----
    for (uint32_t i = 0; i < static_cast<uint32_t>(kRhoBytes); ++i) {
        const uint8_t b = gmEk.GetValue(static_cast<uint32_t>(kEkBytes - kRhoBytes) + i);
        gmRho.SetValue(i, b);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    // YEE 由 Host 启动前清零；本核不预喂最终 y/e

    MarkU32(ws + OFF_PREP_MARK, 0, MAGIC_PREP_MARK);
    MarkU32(ws + OFF_TRACE, SLOT_PREP_DONE, MAGIC_PREP_DONE);
}
