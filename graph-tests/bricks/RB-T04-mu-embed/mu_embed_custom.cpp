/**
 * @file mu_embed_custom.cpp
 * @brief RB-T04：消息嵌入 μ ← Decompress₁(ByteDecode₁(m))。
 *
 * 本文件在流水线中的位置：Encrypt 重建积木 G1 的独立终态探针；Host 读
 * input/m.bin（32B），本核写出 output/mu.bin（256×int32），与 golden 对拍。
 * 对齐 FIPS 203：ByteDecode₁ + Decompress₁；不绑 Encrypt launch。
 *
 * 背景：仓内无独立 μ 探针（KB §A1）；禁止从 alg14 / f203_mu_embed 抄实现。
 * 结论：本砖自写标量 UB 路径；可不建 MIX（无 CrossCore）。
 * 未采用：向量 bit-unpack（d=1 载荷小，标量足够验收契约）。
 */
#include "kernel_operator.h"

namespace {
constexpr int32_t kMsgBytes = 32;   // m ∈ B^32
constexpr int32_t kPolyN = 256;     // 单 poly 系数个数
constexpr int32_t kQ = 3329;        // FIPS 203 模数 q
}  // namespace

/**
 * AIV-only：m[32] uint8 → μ[256] int32（Decompress₁ ∘ ByteDecode₁）。
 * @param mGm  消息字节 GM，dtype uint8，形状 [32]
 * @param muGm 输出多项式 GM，dtype int32，形状 [256]；系数 ∈ {0, 1665}
 * 前置条件：blockDim=1；KERNEL_TYPE_AIV_ONLY；无跨核同步。
 *
 * 数学（FIPS 203）：
 *   ByteDecode₁：系数 i 的 1-bit = (m[i/8] >> (i%8)) & 1（字节内 LSB-first）
 *   Decompress₁(c) = round(c·q/2) ≡ (c·q + 2^{0}) >> 1 = (c·3329 + 1) >> 1
 *   → c=0 得 0；c=1 得 1665。
 */
extern "C" __global__ __aicore__ void mu_embed_custom(GM_ADDR mGm, GM_ADDR muGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    AscendC::GlobalTensor<uint8_t> gmMsg;
    AscendC::GlobalTensor<int32_t> gmMu;
    gmMsg.SetGlobalBuffer((__gm__ uint8_t *)mGm, static_cast<uint32_t>(kMsgBytes));
    gmMu.SetGlobalBuffer((__gm__ int32_t *)muGm, static_cast<uint32_t>(kPolyN));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    // DataCopy 长度按字节对齐：消息 32B 刚好 1 个 32B 块；输出 256×4=1024B。
    pipe.InitBuffer(queIn, 1, static_cast<uint32_t>(kMsgBytes));
    pipe.InitBuffer(queOut, 1, static_cast<uint32_t>(kPolyN) * sizeof(int32_t));

    AscendC::LocalTensor<uint8_t> msgLocal = queIn.AllocTensor<uint8_t>();
    AscendC::LocalTensor<int32_t> muLocal = queOut.AllocTensor<int32_t>();

    // GM → UB：整段消息；PIPE_ALL 保证落地后再标量读。
    AscendC::DataCopy(msgLocal, gmMsg, static_cast<uint32_t>(kMsgBytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    // 逐系数：ByteDecode₁ + Decompress₁（整数舍入式，见 docs/notes Compress-Decompress 指南 P-DEC，d=1）。
    for (int32_t i = 0; i < kPolyN; ++i) {
        const uint32_t byteIdx = static_cast<uint32_t>(i >> 3);   // i / 8
        const uint32_t bitOff = static_cast<uint32_t>(i & 7);     // i % 8
        const uint8_t b = msgLocal.GetValue(byteIdx);
        const int32_t bit = static_cast<int32_t>((b >> bitOff) & 1u);
        // (bit * q + 2^(d-1)) >> d，d=1 → bias=1
        const int32_t mu = (bit * kQ + 1) >> 1;
        muLocal.SetValue(static_cast<uint32_t>(i), mu);
    }

    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmMu, muLocal, static_cast<uint32_t>(kPolyN));

    queIn.FreeTensor(msgLocal);
    queOut.FreeTensor(muLocal);
}
