/**
 * @file f203_encrypt_g4_noise_ws_entry.cpp
 * @brief G4 紧凑 launch：workspace GM + v GM（2 参，规避 SIM 6 参 507000）。
 *
 * workspace 布局见 f203_encrypt_g4_ws_layout.hpp。
 */
#include "f203_encrypt_g4_scalar.hpp"
#include "f203_encrypt_g4_ws_layout.hpp"
#include "kernel_operator.h"

using namespace AscendC;

extern "C" __global__ __aicore__ void f203_encrypt_g4_noise_ws(GM_ADDR wsGm, GM_ADDR vGm)
{
    // ── MIX 占位（仅 SIM/NPU；CPU 保持 AIV-only）──
    // 背景：INTT 是 MIX 核（含 AIC，编入 device.o）；纯 AIV-only 核会被 ascendc_library
    //       单独编入 device_aiv.o，而 CAModel/SIM 下 MIX binary 加载后该 AIV binary
    //       注册失效（RegisterAscendBinary aiv ret 107000 → 其后 launch 全部 507000）。
    // 结论：SIM/NPU 把本核声明为 MIX_AIC_1_2，使其与 INTT 同处 device.o，绕过 device_aiv.o；
    //       AIC 与 AIV subcore!=0 空跑，真正标量加噪仅在 AIV subcore 0 执行（单核语义不变）。
    //       CPU debug 不分 binary 且 MIX subblock 语义不同，保持原 AIV_ONLY 单核路径。
#ifdef ASCENDC_CPU_DEBUG
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() != 0) {
        return;
    }
#else
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    if (GetSubBlockNum() == 1) {
        return; // AIC：不参与计算，仅占位以并入 device.o
    }
    if (GetSubBlockIdx() != 0) {
        return; // 仅 AIV subcore 0 做标量加噪，AIV1 空跑
    }
#endif
    GM_ADDR uGm = wsGm + f203_g4_ws::kUOff;
    GM_ADDR trGm = wsGm + f203_g4_ws::kTrOff;
    GM_ADDR e1Gm = wsGm + f203_g4_ws::kE1Off;
    GM_ADDR e2Gm = wsGm + f203_g4_ws::kE2Off;
    GM_ADDR mGm = wsGm + f203_g4_ws::kMOff;
    f203_g4::add_noise_embed_scalar(uGm, e1Gm, trGm, e2Gm, mGm, vGm);
}

#ifndef __CCE_KT_TEST__
void f203_encrypt_g4_noise_ws_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *wsGm, uint8_t *vGm)
{
    f203_encrypt_g4_noise_ws<<<blockDim, l2ctrl, stream>>>(wsGm, vGm);
}
#endif
