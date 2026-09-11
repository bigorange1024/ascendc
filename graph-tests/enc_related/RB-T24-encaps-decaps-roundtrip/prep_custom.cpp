/**
 * @file prep_custom.cpp
 * @brief RB-T24 Launch1：prep（AIV-only）— ek/ρ + 设备 (K‖r)←G(m‖H(ek))。
 *
 * 相对 T21：不再从 Host 拷 coins；改用 shared SHA3：
 *   h←SHA3-256(ek)；(K‖r)←SHA3-512(m‖h)；写 OFF_H/OFF_K/OFF_COINS。
 * m 已由 Host 预装 OFF_M（同 T21）。结论：blockDim=1、AIV-only；无 CrossCore。
 */
#include "encaps_g_device.hpp"
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
 * Launch1 prep：完整 ek / ρ + 设备 Encaps 头 G/H → K‖coins。
 * @param ekIn [in]  ek_PKE[1568]=BE₁₂(t̂)‖ρ；镜像到 OFF_EK，尾 32B→ρ
 * @param ws   [in/out] 读 OFF_M；写 OFF_EK/OFF_RHO/OFF_COINS/OFF_K/OFF_H/PREP_MARK/TRACE
 * @param tiling 占位
 * 前置：blockDim=1；AIV-only；Host 已写 OFF_M；禁 Host 预喂 coins/K。
 */
extern "C" __global__ __aicore__ void prep_custom(GM_ADDR ekIn, GM_ADDR ws, TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    using namespace tiling;

    AscendC::GlobalTensor<uint8_t> gmEkIn;
    AscendC::GlobalTensor<uint8_t> gmEkWs;
    AscendC::GlobalTensor<uint8_t> gmRho;

    gmEkIn.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ekIn),
                           static_cast<uint32_t>(kEkBytes));
    gmEkWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_EK),
                           static_cast<uint32_t>(kEkBytes));
    gmRho.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_RHO),
                          static_cast<uint32_t>(kRhoBytes));

    // ---- 完整 ek → OFF_EK（Decode₁₂ + H(ek) 同源）----
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

    // ---- 设备 Encaps 头：h←H(ek)；(K‖r)←G(m‖h)；coins=r ----
    // 背景：相对 T21 Host 预喂 coins/K；本刀下沉到 shared Sha3OneShot。
    EncapsGDevice::RunEncapsHeadGFromWs(ws);
    MarkU32(ws + OFF_TRACE, SLOT_AIV0_G_DONE, MAGIC_AIV0_G_DONE);

    MarkU32(ws + OFF_PREP_MARK, 0, MAGIC_PREP_MARK);
    MarkU32(ws + OFF_TRACE, SLOT_PREP_DONE, MAGIC_PREP_DONE);
}
