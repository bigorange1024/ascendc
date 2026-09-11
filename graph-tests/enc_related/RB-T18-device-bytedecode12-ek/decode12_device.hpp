/**
 * @file decode12_device.hpp
 * @brief RB-T18 设备侧薄壳：读 Host ek → ByteDecode₁₂ → t̂[4×256]。
 *
 * 流水线位置：MIX AIV0 在 flag 1/3 握手之后调用；写出 int32[1024]。
 *
 * 积木接线（**不**大段抄 T05 / encrypt）：
 *   - `library/shared/f203_byte_codec/byte_decode12_vec.hpp`：`poly_byte_decode12_scalar_gm`
 *
 * I/O 契约（与 T05 终态一致，外形接 Encrypt prep）：
 *   - 输入：ek[1568]=BE₁₂载荷[1536]‖ρ[32]；只解前 1536B
 *   - 输出：t̂ 平面 [4,256] int32
 *
 * 背景：T05 已验证 Decode₁₂ 标量路径；本刀仅换 MIX 壳。
 * 结论：禁 Gather；禁从 alg14/encrypt 搬文件。
 */
#ifndef RB_T18_DECODE12_DEVICE_HPP
#define RB_T18_DECODE12_DEVICE_HPP

#include "f203_byte_codec/byte_decode12_vec.hpp"
#include "kernel_operator.h"
#include "tiling.h"

#include <cstdint>

namespace rb_t18 {

/**
 * AIV0：从 ws 读 ek 体，逐 poly ByteDecode₁₂，写 tHatOut。
 *
 * @param ws       共享 workspace（OFF_EK 起 1568B；禁含最终 t̂）
 * @param tHatOut  独立输出 GM（Host 对拍；启动前须清零）；布局 poly0..3 连续
 */
__aicore__ inline void ComputeTHatDecode12(GM_ADDR ws, GM_ADDR tHatOut)
{
    using namespace tiling;

    AscendC::GlobalTensor<int32_t> gmOut;
    gmOut.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(tHatOut),
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

} // namespace rb_t18

#endif
