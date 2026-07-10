/**
 * @file f203_tail_compress_byteencode.hpp
 * @brief Alg.14 行 22–24：Compress₁₁/₅ + ByteEncode（ml_kem_1024）。
 *
 * 选型定稿（2026-07-10）：
 *   - Compress：统一整数舍入 + 纯 int32 limb 宽乘向量（抄自 pass-f203-compress-unified-int-vec-k4）
 *   - ByteEncode：标量逐组 pack（`BYTE_ENCODE_D_VEC=1`）
 * 数学：C=⌊2^37/q⌋=41285357=629·2^16+63213；(C·u+2^(36-d))>>(37-d)。
 * 原理：docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md
 * 禁止跨探针 #include，仅抄码。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-1024（k=4）K-PKE.Encrypt；stable-fips203-mlkem-pke-encrypt-k4。
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

/** d=5 统一整数 Compress 向量：bias_hi=2^15，acc_shift=16，末步 mask 5bit。 */
__aicore__ inline void poly_compress_d5_vec(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                            AscendC::LocalTensor<int32_t> &lo, AscendC::LocalTensor<int32_t> &hi,
                                            AscendC::LocalTensor<int32_t> &carry, AscendC::LocalTensor<int32_t> &scratch)
{
    poly_compress_unified_limb_vec(out, in, lo, hi, carry, scratch, 0, 32768, 16);
    mask_low_bits_i32(out, scratch, 5, kPolyLen);
}

/** d=11 统一整数 Compress 向量：bias_hi=2^9，acc_shift=10，末步 mask 11bit。 */
__aicore__ inline void poly_compress_d11_vec(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                             AscendC::LocalTensor<int32_t> &lo, AscendC::LocalTensor<int32_t> &hi,
                                             AscendC::LocalTensor<int32_t> &carry,
                                             AscendC::LocalTensor<int32_t> &scratch)
{
    poly_compress_unified_limb_vec(out, in, lo, hi, carry, scratch, 0, 512, 10);
    mask_low_bits_i32(out, scratch, 11, kPolyLen);
}

/** d=5：8 系数 × 5bit → 5B/组（抄自 pass-f203-byteencode-d-vec-k4::pack_d5_group）。 */
__aicore__ inline void pack_d5_group(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in, uint32_t group)
{
    const uint32_t base = group * 8U;
    const uint8_t t0 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 0U)) & 0x1F);
    const uint8_t t1 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 1U)) & 0x1F);
    const uint8_t t2 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 2U)) & 0x1F);
    const uint8_t t3 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 3U)) & 0x1F);
    const uint8_t t4 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 4U)) & 0x1F);
    const uint8_t t5 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 5U)) & 0x1F);
    const uint8_t t6 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 6U)) & 0x1F);
    const uint8_t t7 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 7U)) & 0x1F);
    const uint32_t byteBase = group * 5U;
    out.SetValue(byteBase + 0U, static_cast<uint8_t>(0xFFu & (t0 | (t1 << 5))));
    out.SetValue(byteBase + 1U, static_cast<uint8_t>(0xFFu & ((t1 >> 3) | (t2 << 2) | (t3 << 7))));
    out.SetValue(byteBase + 2U, static_cast<uint8_t>(0xFFu & ((t3 >> 1) | (t4 << 4))));
    out.SetValue(byteBase + 3U, static_cast<uint8_t>(0xFFu & ((t4 >> 4) | (t5 << 1) | (t6 << 6))));
    out.SetValue(byteBase + 4U, static_cast<uint8_t>(0xFFu & ((t6 >> 2) | (t7 << 3))));
}

/** d=11：8 系数 × 11bit → 11B/组（抄自 pass-f203-byteencode-d-vec-k4::pack_d11_group）。 */
__aicore__ inline void pack_d11_group(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                      uint32_t group)
{
    const uint32_t base = group * 8U;
    const uint16_t t0 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 0U)) & 0x7FF);
    const uint16_t t1 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 1U)) & 0x7FF);
    const uint16_t t2 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 2U)) & 0x7FF);
    const uint16_t t3 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 3U)) & 0x7FF);
    const uint16_t t4 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 4U)) & 0x7FF);
    const uint16_t t5 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 5U)) & 0x7FF);
    const uint16_t t6 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 6U)) & 0x7FF);
    const uint16_t t7 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 7U)) & 0x7FF);
    const uint32_t byteBase = group * 11U;
    out.SetValue(byteBase + 0U, static_cast<uint8_t>((t0 >> 0) & 0xFF));
    out.SetValue(byteBase + 1U, static_cast<uint8_t>((t0 >> 8) | ((t1 << 3) & 0xFF)));
    out.SetValue(byteBase + 2U, static_cast<uint8_t>((t1 >> 5) | ((t2 << 6) & 0xFF)));
    out.SetValue(byteBase + 3U, static_cast<uint8_t>((t2 >> 2) & 0xFF));
    out.SetValue(byteBase + 4U, static_cast<uint8_t>((t2 >> 10) | ((t3 << 1) & 0xFF)));
    out.SetValue(byteBase + 5U, static_cast<uint8_t>((t3 >> 7) | ((t4 << 4) & 0xFF)));
    out.SetValue(byteBase + 6U, static_cast<uint8_t>((t4 >> 4) | ((t5 << 7) & 0xFF)));
    out.SetValue(byteBase + 7U, static_cast<uint8_t>((t5 >> 1) & 0xFF));
    out.SetValue(byteBase + 8U, static_cast<uint8_t>((t5 >> 9) | ((t6 << 2) & 0xFF)));
    out.SetValue(byteBase + 9U, static_cast<uint8_t>((t6 >> 6) | ((t7 << 5) & 0xFF)));
    out.SetValue(byteBase + 10U, static_cast<uint8_t>(t7 >> 3));
}

/** c₂：160B；comp 已由 Compress₅ mask，直接 32 组 pack。 */
__aicore__ inline void poly_byte_encode_d5_local(AscendC::LocalTensor<uint8_t> &out,
                                                 AscendC::LocalTensor<int32_t> &comp)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        pack_d5_group(out, comp, g);
    }
}

/** c₁ 单 poly：352B；comp 已由 Compress₁₁ mask，直接 32 组 pack。 */
__aicore__ inline void poly_byte_encode_d11_local(AscendC::LocalTensor<uint8_t> &out,
                                                  AscendC::LocalTensor<int32_t> &comp)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        pack_d11_group(out, comp, g);
    }
}

} // namespace f203_tail
