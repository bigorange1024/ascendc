#pragma once

/**
 * @file dec_prep_helpers.hpp
 * @brief DRW-D01 本地辅助：ByteDecode_{5,11} 标量 unpack + Decompress_d 向量公式。
 *
 * 本文件在流水线中的位置：仅被 `dec_prep_custom.cpp` include；算法契约对齐
 * FIPS 203 Alg.5/6（bit 打包）与 Eq.4.8（Decompress）；ByteDecode₁₂ 走
 * `library/shared/f203_byte_codec/byte_decode12_vec.hpp`，本头不重复。
 *
 * 背景：Decrypt L1 unpack 需同时支持 d_u=11 与 d_v=5；shared 统一 Decompress
 * 头依赖编译期单一 `F203_UNIFIED_ROUND_D`，无法同 TU 双 d。
 * 结论：本刀本地实现运行期 d/bias 的 Decompress，以及 d=5/11 标量 unpack。
 * 未采用：include 探针目录 `byte_decode_d_vec.hpp` / 抄 T25/alg15 源码。
 */
#include "kernel_operator.h"

namespace dec_prep {

constexpr int32_t kQ = 3329;
constexpr int32_t kPolyN = 256;
constexpr uint32_t kPolyN_u = 256U;
constexpr uint32_t kBytesD5 = 160U;   // 5*256/8
constexpr uint32_t kBytesD11 = 352U;  // 11*256/8
constexpr int32_t kBiasD5 = 16;       // 2^(5-1)
constexpr int32_t kBiasD11 = 1024;    // 2^(11-1)

/**
 * d=5：5B → 8×5bit（Alg.6 逆；组内 bit 跨字节）。
 * @param out UB int32[256] 压缩域系数
 * @param in  UB uint8[160] 打包字节
 * @param group 组号 0..31
 */
__aicore__ inline void unpack_d5_group(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                       uint32_t group)
{
    const uint32_t byteBase = group * 5U;
    const uint8_t b0 = in.GetValue(byteBase + 0U);
    const uint8_t b1 = in.GetValue(byteBase + 1U);
    const uint8_t b2 = in.GetValue(byteBase + 2U);
    const uint8_t b3 = in.GetValue(byteBase + 3U);
    const uint8_t b4 = in.GetValue(byteBase + 4U);
    const uint32_t c = group * 8U;
    out.SetValue(static_cast<int32_t>(c + 0U), static_cast<int32_t>(0x1Fu & (b0 >> 0)));
    out.SetValue(static_cast<int32_t>(c + 1U), static_cast<int32_t>(0x1Fu & ((b0 >> 5) | (b1 << 3))));
    out.SetValue(static_cast<int32_t>(c + 2U), static_cast<int32_t>(0x1Fu & (b1 >> 2)));
    out.SetValue(static_cast<int32_t>(c + 3U), static_cast<int32_t>(0x1Fu & ((b1 >> 7) | (b2 << 1))));
    out.SetValue(static_cast<int32_t>(c + 4U), static_cast<int32_t>(0x1Fu & ((b2 >> 4) | (b3 << 4))));
    out.SetValue(static_cast<int32_t>(c + 5U), static_cast<int32_t>(0x1Fu & (b3 >> 1)));
    out.SetValue(static_cast<int32_t>(c + 6U), static_cast<int32_t>(0x1Fu & ((b3 >> 6) | (b4 << 2))));
    out.SetValue(static_cast<int32_t>(c + 7U), static_cast<int32_t>(0x1Fu & (b4 >> 3)));
}

/**
 * d=11：11B → 8×11bit（ML-KEM-1024 c₁）。
 * @param out UB int32[256] 压缩域系数
 * @param in  UB uint8[352] 打包字节
 * @param group 组号 0..31
 */
__aicore__ inline void unpack_d11_group(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                        uint32_t group)
{
    const uint32_t byteBase = group * 11U;
    const uint8_t b0 = in.GetValue(byteBase + 0U);
    const uint8_t b1 = in.GetValue(byteBase + 1U);
    const uint8_t b2 = in.GetValue(byteBase + 2U);
    const uint8_t b3 = in.GetValue(byteBase + 3U);
    const uint8_t b4 = in.GetValue(byteBase + 4U);
    const uint8_t b5 = in.GetValue(byteBase + 5U);
    const uint8_t b6 = in.GetValue(byteBase + 6U);
    const uint8_t b7 = in.GetValue(byteBase + 7U);
    const uint8_t b8 = in.GetValue(byteBase + 8U);
    const uint8_t b9 = in.GetValue(byteBase + 9U);
    const uint8_t b10 = in.GetValue(byteBase + 10U);
    const uint16_t t0 = static_cast<uint16_t>(0x7FFu & ((b0 >> 0) | (static_cast<uint16_t>(b1) << 8)));
    const uint16_t t1 = static_cast<uint16_t>(0x7FFu & ((b1 >> 3) | (static_cast<uint16_t>(b2) << 5)));
    const uint16_t t2 = static_cast<uint16_t>(
        0x7FFu & ((b2 >> 6) | (static_cast<uint16_t>(b3) << 2) | (static_cast<uint16_t>(b4) << 10)));
    const uint16_t t3 = static_cast<uint16_t>(0x7FFu & ((b4 >> 1) | (static_cast<uint16_t>(b5) << 7)));
    const uint16_t t4 = static_cast<uint16_t>(0x7FFu & ((b5 >> 4) | (static_cast<uint16_t>(b6) << 4)));
    const uint16_t t5 = static_cast<uint16_t>(
        0x7FFu & ((b6 >> 7) | (static_cast<uint16_t>(b7) << 1) | (static_cast<uint16_t>(b8) << 9)));
    const uint16_t t6 = static_cast<uint16_t>(0x7FFu & ((b8 >> 2) | (static_cast<uint16_t>(b9) << 6)));
    const uint16_t t7 = static_cast<uint16_t>(0x7FFu & ((b9 >> 5) | (static_cast<uint16_t>(b10) << 3)));
    const uint32_t c = group * 8U;
    out.SetValue(static_cast<int32_t>(c + 0U), static_cast<int32_t>(t0));
    out.SetValue(static_cast<int32_t>(c + 1U), static_cast<int32_t>(t1));
    out.SetValue(static_cast<int32_t>(c + 2U), static_cast<int32_t>(t2));
    out.SetValue(static_cast<int32_t>(c + 3U), static_cast<int32_t>(t3));
    out.SetValue(static_cast<int32_t>(c + 4U), static_cast<int32_t>(t4));
    out.SetValue(static_cast<int32_t>(c + 5U), static_cast<int32_t>(t5));
    out.SetValue(static_cast<int32_t>(c + 6U), static_cast<int32_t>(t6));
    out.SetValue(static_cast<int32_t>(c + 7U), static_cast<int32_t>(t7));
}

/** 整 poly ByteDecode₅：UB 字节 → UB 压缩域系数。 */
__aicore__ inline void poly_byte_decode_d5_local(AscendC::LocalTensor<int32_t> &out,
                                                 AscendC::LocalTensor<uint8_t> &in)
{
    for (uint32_t g = 0; g < kPolyN_u / 8U; ++g) {
        unpack_d5_group(out, in, g);
    }
}

/** 整 poly ByteDecode₁₁：UB 字节 → UB 压缩域系数。 */
__aicore__ inline void poly_byte_decode_d11_local(AscendC::LocalTensor<int32_t> &out,
                                                  AscendC::LocalTensor<uint8_t> &in)
{
    for (uint32_t g = 0; g < kPolyN_u / 8U; ++g) {
        unpack_d11_group(out, in, g);
    }
}

/**
 * Decompress_d：out = (in * q + bias) >> d（FIPS Eq.4.8 整数形）。
 * @param out/in/tmp 各 UB int32[256]
 * @param dBits 右移位数（5 或 11）
 * @param bias  2^(d-1)
 */
__aicore__ inline void poly_decompress_d_local(AscendC::LocalTensor<int32_t> &out,
                                               AscendC::LocalTensor<int32_t> &in,
                                               AscendC::LocalTensor<int32_t> &tmp, int32_t dBits, int32_t bias)
{
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    const int32_t n = kPolyN;
    Muls(tmp, in, kQ, n);
    Adds(tmp, tmp, bias, n);
    ShiftRight(out, tmp, dBits, n);
}

}  // namespace dec_prep
