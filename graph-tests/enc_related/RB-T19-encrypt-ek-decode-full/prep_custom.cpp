/**
 * @file prep_custom.cpp
 * @brief RB-T19 Launch1：prep（AIV-only）— 完整 ek→OFF_EK；ρ←ek 尾；coins→OFF_COINS。
 *
 * 相对 T17：额外把完整 ek[1568] 写入 ws，供 Launch2 ByteDecode₁₂（禁 Host 预喂 t̂）。
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
 * Launch1 prep：coins / 完整 ek / ρ 写入共享 workspace。
 * @param coinsIn [in]  Host coins[32]
 * @param ekIn    [in]  ek_PKE[1568]=BE₁₂(t̂)‖ρ；本核整份镜像到 OFF_EK，并取尾 32B 为 ρ
 * @param ws      [in/out] 写 OFF_COINS / OFF_EK / OFF_RHO / OFF_PREP_MARK / TRACE
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
    AscendC::GlobalTensor<uint8_t> gmEkIn;
    AscendC::GlobalTensor<uint8_t> gmEkWs;
    AscendC::GlobalTensor<uint8_t> gmRho;

    gmCoinsIn.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(coinsIn),
                              static_cast<uint32_t>(kCoinsBytes));
    gmCoinsWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_COINS),
                              static_cast<uint32_t>(kCoinsBytes));
    gmEkIn.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ekIn),
                           static_cast<uint32_t>(kEkBytes));
    gmEkWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_EK),
                           static_cast<uint32_t>(kEkBytes));
    gmRho.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_RHO),
                          static_cast<uint32_t>(kRhoBytes));

    // ---- coins → ws（设备侧可见；禁最终 y/e）----
    for (uint32_t i = 0; i < static_cast<uint32_t>(kCoinsBytes); ++i) {
        gmCoinsWs.SetValue(i, gmCoinsIn.GetValue(i));
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---- 完整 ek → OFF_EK（Launch2 Decode₁₂ 读体；ρ 尾一并镜像）----
    for (uint32_t i = 0; i < static_cast<uint32_t>(kEkBytes); ++i) {
        gmEkWs.SetValue(i, gmEkIn.GetValue(i));
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---- ρ ← ek 尾 32B ----
    for (uint32_t i = 0; i < static_cast<uint32_t>(kRhoBytes); ++i) {
        const uint8_t b = gmEkIn.GetValue(static_cast<uint32_t>(kEkBytes - kRhoBytes) + i);
        gmRho.SetValue(i, b);
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    MarkU32(ws + OFF_PREP_MARK, 0, MAGIC_PREP_MARK);
    MarkU32(ws + OFF_TRACE, SLOT_PREP_DONE, MAGIC_PREP_DONE);
}
