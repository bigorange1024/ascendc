/**
 * @file decode12_device.hpp
 * @brief RB-D05 设备侧薄壳：读 ws 内 ek → ByteDecode₁₂ → t̂[4×256]（写 OFF_T_HAT）。
 *
 * 流水线位置：Launch2 MIX AIV0，flag 1/3 握手之后、CBD/Mul 之前。
 *
 * 积木接线（**不**大段抄 T05 / encrypt；契约对齐 T18）：
 *   - `library/shared/f203_byte_codec/byte_decode12_vec.hpp`：`poly_byte_decode12_scalar_gm`
 *
 * I/O 契约：
 *   - 输入：OFF_EK 起 ek[1568]=BE₁₂载荷[1536]‖ρ[32]；只解前 1536B
 *   - 输出：OFF_T_HAT 平面 [4,256] int32（Host 启动前清零；禁预喂最终 t̂）
 *
 * 背景：T05/T18 已验证 Decode₁₂ 标量路径；本刀嵌进 T17 全链。
 * 结论：禁 Gather；禁从 alg14/encrypt/frozen 搬文件。
 */
#ifndef RB_D05_DECODE12_DEVICE_HPP
#define RB_D05_DECODE12_DEVICE_HPP

#include "f203_byte_codec/byte_decode12_vec.hpp"
#include "kernel_operator.h"
#include "tiling.h"

#include <cstdint>

namespace reenc_bd12 {

/**
 * AIV0：从 ws+OFF_EK 读 ek 体，逐 poly ByteDecode₁₂，写 ws+OFF_T_HAT。
 *
 * @param ws  共享 workspace（含 OFF_EK；禁含最终 t̂ 预填）
 */
__aicore__ inline void ComputeTHatDecode12(GM_ADDR ws)
{
    using namespace tiling;

    AscendC::GlobalTensor<int32_t> gmOut;
    gmOut.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_T_HAT),
                          static_cast<uint32_t>(kTHatCoeffs));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    pipe.InitBuffer(queOut, 1, static_cast<uint32_t>(kPolyN) * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> outLocal = queOut.AllocTensor<int32_t>();

    // 逐 poly：shared 标量路径直接从 GM 读字节；解完后搬出该 poly 的 256 系数。
    for (int32_t p = 0; p < kKem; ++p) {
        const uint32_t byteOff = static_cast<uint32_t>(p) * static_cast<uint32_t>(kPolyPackedBytes);
        const __gm__ uint8_t *row =
            reinterpret_cast<__gm__ uint8_t *>(ws + OFF_EK) + byteOff;
        f203_byte_codec::poly_byte_decode12_scalar_gm(outLocal, row, kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmOut[static_cast<uint32_t>(p * kPolyN)], outLocal,
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    queOut.FreeTensor(outLocal);
    (void)kEkBytes; // 契约：完整 ek 含 ρ 尾；本函数不读 ρ
}

} // namespace reenc_bd12

#endif
