/**
 * @file mu_embed_device.hpp
 * @brief RB-D05 设备侧：μ ← Decompress₁∘ByteDecode₁(m)（T04 契约）。
 *
 * 流水线位置：Launch2 MIX AIV0，握手 Wait(3) 之后、INTT+噪之前写 OFF_MU。
 * 读 Host 预装的原始 m'[32]（OFF_M）；**禁** Host 预喂最终 μ。
 *
 * 背景：Decaps Re-Encrypt 的 μ 源是 Decrypt 终产物 m'；设备嵌入。
 * 结论：标量 UB 路径；与 T04 数学一致；不建独立 launch。
 * 未采用：向量 bit-unpack（d=1 载荷小）。
 */
#ifndef RB_D05_MU_EMBED_DEVICE_HPP
#define RB_D05_MU_EMBED_DEVICE_HPP

#include "kernel_operator.h"
#include "tiling.h"

namespace reenc_mu {

/**
 * AIV：读 ws+OFF_M 的 m[32] uint8 → 写 ws+OFF_MU 的 μ[256] int32。
 * @param ws 共享 workspace（Host 已装 m；本函数写最终 μ）
 * 前置：blockIdx/subBlock 由调用方保证仅 AIV0 调用；无 CrossCore。
 *
 * 数学（FIPS 203 / T04）：
 *   ByteDecode₁：bit_i = (m[i/8] >> (i%8)) & 1（字节内 LSB-first）
 *   Decompress₁(c) = (c·q + 1) >> 1 → {0, 1665}
 */
__aicore__ inline void ComputeMuEmbedFromM(GM_ADDR ws)
{
    using namespace tiling;

    AscendC::GlobalTensor<uint8_t> gmMsg;
    AscendC::GlobalTensor<int32_t> gmMu;
    gmMsg.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_M),
                          static_cast<uint32_t>(kMsgBytes));
    gmMu.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_MU),
                         static_cast<uint32_t>(kPolyN));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    // DataCopy 按字节：消息 32B；输出 256×4=1024B。
    pipe.InitBuffer(queIn, 1, static_cast<uint32_t>(kMsgBytes));
    pipe.InitBuffer(queOut, 1, static_cast<uint32_t>(kPolyN) * sizeof(int32_t));

    AscendC::LocalTensor<uint8_t> msgLocal = queIn.AllocTensor<uint8_t>();
    AscendC::LocalTensor<int32_t> muLocal = queOut.AllocTensor<int32_t>();

    AscendC::DataCopy(msgLocal, gmMsg, static_cast<uint32_t>(kMsgBytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    // 逐系数：ByteDecode₁ + Decompress₁（d=1，bias=1）
    for (int32_t i = 0; i < kPolyN; ++i) {
        const uint32_t byteIdx = static_cast<uint32_t>(i >> 3);
        const uint32_t bitOff = static_cast<uint32_t>(i & 7);
        const uint8_t b = msgLocal.GetValue(byteIdx);
        const int32_t bit = static_cast<int32_t>((b >> bitOff) & 1u);
        const int32_t mu = (bit * kQ + 1) >> 1;
        muLocal.SetValue(static_cast<uint32_t>(i), mu);
    }

    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmMu, muLocal, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();

    queIn.FreeTensor(msgLocal);
    queOut.FreeTensor(muLocal);
}

} // namespace reenc_mu

#endif
