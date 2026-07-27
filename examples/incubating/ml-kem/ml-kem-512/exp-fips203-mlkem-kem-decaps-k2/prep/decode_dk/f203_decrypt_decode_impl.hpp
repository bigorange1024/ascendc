/**
 * @file f203_decrypt_decode_impl.hpp
 * @brief G2a：dk → ŝ（ByteDecode₁₂×2）。Decode 标量；落盘须 DataCopy（禁标量写 GM）。
 *
 * 背景：单 kernel 融合时，同 launch 内标量写 GM → 后续 MTE DataCopy 读在 SIM 上不可见
 *（Encrypt R2 / docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md）。
 * 结论：系数先填 UB（SetValue），再 DataCopy→GM；未采用标量直写 sHatGm。
 */
#ifndef F203_DECRYPT_DECODE_IMPL_HPP
#define F203_DECRYPT_DECODE_IMPL_HPP

#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

namespace decrypt_g4 {

constexpr int32_t kDecodeK = static_cast<int32_t>(F203_DECRYPT_K);
constexpr int32_t kDecodeN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kDecodePolyBytes = 384;

/**
 * 单 poly ByteDecode₁₂：384B → 256 个 12-bit 系数。
 * 每 3 字节拆出 2 个系数（低 12 / 高 12）。
 */
__aicore__ inline void poly_byte_decode12_local(int32_t *out, const uint8_t *in)
{
    for (int32_t i = 0; i < kDecodeN / 2; ++i) {
        const int32_t b0 = static_cast<int32_t>(in[3 * i]);
        const int32_t b1 = static_cast<int32_t>(in[3 * i + 1]);
        const int32_t b2 = static_cast<int32_t>(in[3 * i + 2]);
        out[2 * i] = b0 | ((b1 & 0x0F) << 8);
        out[2 * i + 1] = (b1 >> 4) | (b2 << 4);
    }
}

/**
 * Alg.15：ŝ ← ByteDecode₁₂(dk_PKE)。
 * @param dkGm    输入 dk_PKE（768B = 2×384）
 * @param sHatGm  输出 ŝ polyvec [k×N] int32
 * 前置：仅 AIV0；系数经 UB SetValue 再 DataCopy→GM（禁标量直写 GM）。
 */
__aicore__ inline void decode_s_hat_impl(GM_ADDR dkGm, GM_ADDR sHatGm)
{
    const auto *dkBytes = reinterpret_cast<const __gm__ uint8_t *>(dkGm);
    AscendC::GlobalTensor<int32_t> sGm;
    sGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(sHatGm),
                        static_cast<uint32_t>(kDecodeK * kDecodeN));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    const uint32_t bytes = static_cast<uint32_t>(kDecodeN) * static_cast<uint32_t>(sizeof(int32_t));
    pipe.InitBuffer(queOut, 1, bytes);
    AscendC::LocalTensor<int32_t> outLocal = queOut.AllocTensor<int32_t>();

    for (int32_t j = 0; j < kDecodeK; ++j) {
        /* 读第 j 个 poly 的 384B 编码 */
        uint8_t polyBuf[kDecodePolyBytes];
        for (int32_t b = 0; b < kDecodePolyBytes; ++b) {
            polyBuf[b] = dkBytes[j * kDecodePolyBytes + b];
        }
        int32_t coeffs[kDecodeN];
        poly_byte_decode12_local(coeffs, polyBuf);
        /* 填 UB 再 DataCopy，避免同 launch 标量写 GM 对后续 MTE 不可见 */
        for (int32_t c = 0; c < kDecodeN; ++c) {
            outLocal.SetValue(c, coeffs[c]);
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(sGm[static_cast<uint32_t>(j) * static_cast<uint32_t>(kDecodeN)], outLocal,
                          static_cast<uint32_t>(kDecodeN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    queOut.FreeTensor(outLocal);
}

} // namespace decrypt_g4

#endif
