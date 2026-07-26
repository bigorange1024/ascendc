/**
 * @file f203_tail_compress_byteencode.hpp
 * @brief Alg.14 行 22–24：Compress₁₀/₄ + ByteEncode（ML-KEM-768）。
 *
 * 选型定稿（2026-07-10）：
 *   - Compress：统一整数舍入 + 纯 int32 limb 宽乘向量（复用统一整数公式）
 *   - ByteEncode：标量逐组 pack（`BYTE_ENCODE_D_VEC=1`）
 * 数学：C=⌊2^37/q⌋=41285357=629·2^16+63213；(C·u+2^(36-d))>>(37-d)。
 * 原理：docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md
 * 禁止跨探针 #include，仅抄码。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-768（k=3）K-PKE.Encrypt。
 * 与 golden：最终对拍 output/c.bin。
 */
#pragma once

#include "f203_encrypt_tail_layout.h"
#include "kernel_operator.h"

namespace f203_tail {

constexpr uint32_t kPolyLen = F203_TAIL_N;

// 统一 Compress 乘数 limb 拆分（全 d 共用；C=41285357=629·65536+63213）
constexpr int32_t kUnifiedCompressC0 = 63213;
constexpr int32_t kUnifiedCompressC1 = 629;
constexpr int32_t kUnifiedLimbShift = 16;

/** v ← v mod 2^bits（移位+乘+减；lo 为正时等价于 lo & (2^bits-1)）。 */
__aicore__ inline void mask_low_bits_i32(AscendC::LocalTensor<int32_t> &v, AscendC::LocalTensor<int32_t> &tmp,
                                         int32_t bits, uint32_t count)
{
    using AscendC::Muls;
    using AscendC::ShiftRight;
    using AscendC::Sub;
    const int32_t n = static_cast<int32_t>(count);
    const int32_t scale = static_cast<int32_t>(1) << bits;
    ShiftRight(tmp, v, bits, n);
    Muls(tmp, tmp, scale, n);
    Sub(v, v, tmp, n);
}

/**
 * 统一整数 Compress limb 宽乘核心（d 无关；bias/accShift 由调用方按 d 填入）。
 *
 * carry = (lo>>16) + bias_hi + ((lo mod 2^16) + bias_lo) >> 16
 * out   = (hi + carry) >> accShift
 *
 * @param lo/hi/carry/scratch 各长 kPolyLen 的独立 UB，勿与 out 别名。
 */
__aicore__ inline void poly_compress_unified_limb_vec(AscendC::LocalTensor<int32_t> &out,
                                                      AscendC::LocalTensor<int32_t> &in,
                                                      AscendC::LocalTensor<int32_t> &lo,
                                                      AscendC::LocalTensor<int32_t> &hi,
                                                      AscendC::LocalTensor<int32_t> &carry,
                                                      AscendC::LocalTensor<int32_t> &scratch, int32_t biasLo,
                                                      int32_t biasHi, int32_t accShift)
{
    using AscendC::Add;
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    const int32_t n = static_cast<int32_t>(kPolyLen);

    Muls(lo, in, kUnifiedCompressC0, n);
    Muls(hi, in, kUnifiedCompressC1, n);

    ShiftRight(carry, lo, kUnifiedLimbShift, n);
    mask_low_bits_i32(lo, scratch, kUnifiedLimbShift, kPolyLen);
    Adds(lo, lo, biasLo, n);
    ShiftRight(scratch, lo, kUnifiedLimbShift, n);
    Adds(carry, carry, biasHi, n);
    Add(carry, carry, scratch, n);
    Add(hi, hi, carry, n);
    ShiftRight(out, hi, accShift, n);
}

/** d=4 统一整数 Compress 向量：bias=2^32，acc_shift=17，末步 mask 4bit。 */
__aicore__ inline void poly_compress_d4_vec(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                            AscendC::LocalTensor<int32_t> &lo, AscendC::LocalTensor<int32_t> &hi,
                                            AscendC::LocalTensor<int32_t> &carry, AscendC::LocalTensor<int32_t> &scratch)
{
    poly_compress_unified_limb_vec(out, in, lo, hi, carry, scratch, 0, 65536, 17);
    mask_low_bits_i32(out, scratch, 4, kPolyLen);
}

/** d=10 统一整数 Compress 向量：bias=2^26，acc_shift=11，末步 mask 10bit。 */
__aicore__ inline void poly_compress_d10_vec(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                             AscendC::LocalTensor<int32_t> &lo, AscendC::LocalTensor<int32_t> &hi,
                                             AscendC::LocalTensor<int32_t> &carry,
                                             AscendC::LocalTensor<int32_t> &scratch)
{
    poly_compress_unified_limb_vec(out, in, lo, hi, carry, scratch, 0, 1024, 11);
    mask_low_bits_i32(out, scratch, 10, kPolyLen);
}

/** 通用 ByteEncode_d：8 个系数按 FIPS 小端 bit 序打包到 d 字节。 */
__aicore__ inline void pack_d_group(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                    uint32_t group, uint32_t d)
{
    const uint32_t base = group * 8U;
    const uint32_t byteBase = group * d;
    for (uint32_t b = 0U; b < d; ++b) {
        out.SetValue(byteBase + b, 0U);
    }
    for (uint32_t i = 0U; i < 8U; ++i) {
        uint32_t val = static_cast<uint32_t>(in.GetValue(static_cast<int32_t>(base + i))) & ((1U << d) - 1U);
        for (uint32_t bit = 0U; bit < d; ++bit) {
            if (((val >> bit) & 1U) != 0U) {
                const uint32_t pos = i * d + bit;
                const uint32_t outByte = byteBase + (pos >> 3U);
                const uint8_t old = out.GetValue(outByte);
                out.SetValue(outByte, static_cast<uint8_t>(old | static_cast<uint8_t>(1U << (pos & 7U))));
            }
        }
    }
}

/** c₂：128B；comp 已由 Compress₄ mask，直接 32 组 pack。 */
__aicore__ inline void poly_byte_encode_d4_local(AscendC::LocalTensor<uint8_t> &out,
                                                 AscendC::LocalTensor<int32_t> &comp)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        pack_d_group(out, comp, g, 4U);
    }
}

/** c₁ 单 poly：320B；comp 已由 Compress₁₀ mask，直接 32 组 pack。 */
__aicore__ inline void poly_byte_encode_d10_local(AscendC::LocalTensor<uint8_t> &out,
                                                  AscendC::LocalTensor<int32_t> &comp)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        pack_d_group(out, comp, g, 10U);
    }
}

} // namespace f203_tail
