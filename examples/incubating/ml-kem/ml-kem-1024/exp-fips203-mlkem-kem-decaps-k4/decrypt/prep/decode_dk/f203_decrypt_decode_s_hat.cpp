/**
 * @file f203_decrypt_decode_s_hat.cpp
 * @brief Decrypt 流水线（1-kernel fused）G2a 独立入口：dk_pke GM → s_hat GM。
 *
 * 对齐 FIPS 203 Alg.15：ŝ ← ByteDecode₁₂(dk_PKE)，k=4 时共 4×384B → 4×256 个 int32 系数。
 * 本文件为**分段探针 / 调试 launch**用的独立 kernel；生产融合路径走
 * `decrypt_g4::decode_s_hat_impl`（见 f203_decrypt_decode_impl.hpp），后者用 UB SetValue +
 * DataCopy 落盘，避免同 launch 标量写 GM 对后续 MTE 不可见。
 *
 * golden I/O：输入 `input/dk_pke.bin`（1536B）；本段输出中间态 ŝ[k×N] int32（生产路径不落盘）。
 */
#include "kernel_operator.h"
#include "f203_decrypt_layout.h"

using namespace AscendC;

namespace {

constexpr int32_t kK = static_cast<int32_t>(F203_DECRYPT_K);
constexpr int32_t kN = static_cast<int32_t>(F203_DECRYPT_N);
/** 单 poly ByteEncode₁₂ / ByteDecode₁₂ 字节数：256×12/8 = 384。 */
constexpr int32_t kPolyBytes = 384;
/** dk_PKE 总字节（k×384）；本函数体未直接用，保留与布局常量对齐。 */
constexpr int32_t kDkBytes = static_cast<int32_t>(F203_DK_PKE_BYTES);

/**
 * 单 poly ByteDecode₁₂：384B → 256 个 12-bit 系数（存为 int32）。
 * @param out 输出系数 [N]；@param in 编码字节 [384]
 *
 * 每 3 字节拆 2 个系数（FIPS 203 ByteDecode₁₂）：
 *   b0 | ((b1 & 0x0F)<<8) → 偶下标； (b1>>4) | (b2<<4) → 奇下标。
 */
__aicore__ inline void poly_byte_decode12_local(int32_t *out, const uint8_t *in)
{
    for (int32_t i = 0; i < kN / 2; ++i) {
        const int32_t b0 = static_cast<int32_t>(in[3 * i]);
        const int32_t b1 = static_cast<int32_t>(in[3 * i + 1]);
        const int32_t b2 = static_cast<int32_t>(in[3 * i + 2]);
        out[2 * i] = b0 | ((b1 & 0x0F) << 8);
        out[2 * i + 1] = (b1 >> 4) | (b2 << 4);
    }
}

} // namespace

/**
 * 独立 kernel：dk_PKE → ŝ polyvec。
 * @param dkGm   输入 dk_PKE（1536B = 4×384）
 * @param sHatGm 输出 ŝ [k×N] int32，行主序 poly 拼接
 *
 * 前置：仅 blockIdx==0；SIM/NPU 上 MIX 时 AIC 子块直接 return。
 * 注意：本入口对 GM 使用标量读写，适合**独立 launch**对拍；融合单 kernel 须用 decode_s_hat_impl。
 */
extern "C" __global__ __aicore__ void f203_decrypt_decode_s_hat(GM_ADDR dkGm, GM_ADDR sHatGm)
{
#if defined(ASCENDC_CPU_DEBUG)
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
#else
    /* SIM/NPU：声明 MIX，但本段仅 AIV 干活；AIC（subBlockNum==1）空返回 */
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);
    if (AscendC::GetSubBlockNum() == 1) {
        return;
    }
#endif
    if (GetBlockIdx() != 0) {
        return;
    }

    const auto *dkBytes = reinterpret_cast<const __gm__ uint8_t *>(dkGm);
    auto *sFlat = reinterpret_cast<__gm__ int32_t *>(sHatGm);

    /* 逐 poly：读 384B → Decode₁₂ → 写回 ŝ[j·N .. j·N+N) */
    for (int32_t j = 0; j < kK; ++j) {
        uint8_t polyBuf[384];
        for (int32_t b = 0; b < kPolyBytes; ++b) {
            polyBuf[b] = dkBytes[j * kPolyBytes + b];
        }
        int32_t coeffs[256];
        poly_byte_decode12_local(coeffs, polyBuf);
        for (int32_t c = 0; c < kN; ++c) {
            sFlat[j * kN + c] = coeffs[c];
        }
    }
    (void)kDkBytes;
}

#ifndef __CCE_KT_TEST__
/** Host 侧 launch 包装：blockDim / stream 透传给设备入口。 */
void f203_decrypt_decode_s_hat_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *dkGm, uint8_t *sHatGm)
{
    f203_decrypt_decode_s_hat<<<blockDim, l2ctrl, stream>>>(dkGm, sHatGm);
}
#endif
