#pragma once

/**
 * @file f203_tail_compress_byteencode.hpp
 * @brief Alg.14 行 22–24：Compress₁₁/₅ + ByteEncode（ml_kem_1024）。
 *
 * Compress 向量路径 vendored 自 pass-f203-compress-d-vec-k4（2026-07-08）；
 * ByteEncode d=5/d=11 分组 pack vendored 自 pass-f203-byteencode-d-vec-k4（2026-07-08）；
 * 禁止跨探针 #include，仅抄码。
 *
 * d=5：int32 Barrett（Muls 1290176 + bias 1<<26 + >>27）
 * d=11：cast_div 商（Muls(2^11) + Adds(q/2) + Cast→Div→CAST_TRUNC）
 * ByteEncode：8 系数/组 标量 pack（O(N/8)；与 Alg.5 比特流 0-diff）
 */
#include "f203_encrypt_tail_layout.h"
#include "kernel_operator.h"

namespace f203_tail {

constexpr uint32_t kPolyLen = F203_TAIL_N;
constexpr int32_t kMlKemQ = F203_TAIL_Q;

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

/** d=5 Barrett 向量（抄自 pass-f203-compress-d-vec-k4::poly_compress_barrett_vec）。 */
__aicore__ inline void poly_compress_d5_vec(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                            AscendC::LocalTensor<int32_t> &tmp)
{
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    const int32_t n = static_cast<int32_t>(kPolyLen);
    Muls(out, in, static_cast<int32_t>(1290176), n);
    Adds(out, out, static_cast<int32_t>(1 << 26), n);
    ShiftRight(out, out, 27, n);
    mask_low_bits_i32(out, tmp, 5, kPolyLen);
}

/**
 * d=11 cast_div 商向量（抄自 pass-f203-compress-d-vec-k4::poly_compress_cast_div_vec）。
 * round(u·2^11/q) = floor((u·2048 + q/2)/q)
 */
__aicore__ inline void poly_compress_d11_vec(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                             AscendC::LocalTensor<int32_t> &tmp_i, AscendC::LocalTensor<float> &fRaw,
                                             AscendC::LocalTensor<float> &fTmp, AscendC::LocalTensor<float> &fQuot)
{
    using AscendC::Adds;
    using AscendC::Cast;
    using AscendC::Div;
    using AscendC::Duplicate;
    using AscendC::Muls;
    const int32_t n = static_cast<int32_t>(kPolyLen);
    constexpr int32_t kScale = 1 << 11;
    constexpr int32_t kRoundBias = kMlKemQ / 2;

    Muls(tmp_i, in, kScale, n);
    Adds(tmp_i, tmp_i, kRoundBias, n);
    Cast(fRaw, tmp_i, AscendC::RoundMode::CAST_NONE, static_cast<uint32_t>(n));
    Duplicate(tmp_i, kMlKemQ, n);
    Cast(fTmp, tmp_i, AscendC::RoundMode::CAST_NONE, static_cast<uint32_t>(n));
    Div(fQuot, fRaw, fTmp, n);
    Cast(out, fQuot, AscendC::RoundMode::CAST_TRUNC, static_cast<uint32_t>(n));
    mask_low_bits_i32(out, tmp_i, 11, kPolyLen);
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
