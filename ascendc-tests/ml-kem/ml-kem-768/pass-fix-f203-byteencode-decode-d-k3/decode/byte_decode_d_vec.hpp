#ifndef BYTE_DECODE_D_VEC_HPP
#define BYTE_DECODE_D_VEC_HPP

/**
 * @file byte_decode_d_vec.hpp
 * @brief FIPS 203 Alg.6 ByteDecode_d（d=4/5/10/11）；输出 d-bit 系数，**不含** Decompress。
 *
 * 宏分层（详见 docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md）：
 *   BYTE_DECODE_D_VEC=0/1 — d=5/10/11：**同体**，均为标量 unpack_*_group 逐组（O(N/8)）
 *   BYTE_DECODE_D_VEC=1 — d=4 额外：向量 nibble mask + 标量 scatter（默认）
 *   无 VEC=2（对称 encode VEC=2 未做；预期 Gather 开销 > 标量逐组）
 * 下游 Decompress 默认向量：pass-f203-decompress-d-vec-k4（DECOMPRESS_D_VEC=1）。
 */
#include "byte_decode_d_config.hpp"
#include "f203_mlkem_params.h"
#include "kernel_operator.h"

namespace byte_decode_d {

/** 单个多项式的系数个数（=F203_MLKEM_N=256），决定各 d 值下的分组数（N/8 或 N/4）。 */
constexpr uint32_t kPolyLen = static_cast<uint32_t>(F203_MLKEM_N);
/** 单个多项式解码前的输入字节数，随当前编译期 F203_BYTE_DECODE_D 取值（见 config 头）。 */
constexpr uint32_t kInBytes = static_cast<uint32_t>(F203_BYTE_DECODE_POLY_BYTES);

/**
 * 向量化「取低 bits 位」：v[i] = v[i] & ((1<<bits)-1)，就地修改 v。
 * 用移位+乘+减代替按位与（AscendC 向量 API 无直接 int32 AND，故用等价算术：
 * v - (v>>bits)*2^bits == v & (2^bits-1)，因为 (v>>bits)*2^bits 恰好是 v 的高位部分）。
 * 本探针仅在 d=4 向量 nibble 路径使用（对「宽化后的字节值」取低 4bit，即取偶数位系数）。
 * @param v     [in,out] UB int32[count]，待掩码的数值（原地更新）
 * @param tmp   UB int32[count] 中间缓冲，供 ShiftRight/Muls 暂存移位结果
 * @param bits  掩码位宽（d=4 场景固定传入 4）
 * @param count 参与运算的元素个数（d=4 场景为 kPolyLen/2=128，即字节对数）
 * 前置条件：v 中每元素为非负数，移位为逻辑右移语义。
 */
__aicore__ inline void mask_low_bits_i32(AscendC::LocalTensor<int32_t> &v, AscendC::LocalTensor<int32_t> &tmp,
                                         int32_t bits, uint32_t count)
{
    using AscendC::Muls;
    using AscendC::ShiftRight;
    using AscendC::Sub;
    const int32_t n = static_cast<int32_t>(count);
    const int32_t scale = static_cast<int32_t>(1) << bits;
    /* tmp = v >> bits（v 的高位部分，右移 bits 位后的商）。 */
    ShiftRight(tmp, v, bits, n);
    /* tmp = tmp * 2^bits，还原出 v 的高位部分对齐回原位宽后的整数值。 */
    Muls(tmp, tmp, scale, n);
    /* v = v - tmp，即减去高位部分，只留下低 bits 位，等价于 v & (2^bits-1)。 */
    Sub(v, v, tmp, n);
}

/**
 * 把 uint8 数组逐元素宽化（零扩展）为 int32 数组，供后续向量算术（ShiftRight/Muls/Sub 等
 * 均要求 int32 操作数）使用。逐元素标量 GetValue/SetValue，非真正的向量 widen intrinsic。
 * @param out   [out] UB int32[count]，宽化结果
 * @param in    UB uint8[count]，原始字节数据
 * @param count 元素个数
 */
__aicore__ inline void widen_bytes_to_i32(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                          uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        out.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(in.GetValue(i)));
    }
}

#if F203_BYTE_DECODE_D == 4

/**
 * d=4 单字节标量拆分：1 字节 → 2 个 4bit 系数（低 4bit 为偶数位，高 4bit 为奇数位）。
 * @param out     [out] UB int32[kPolyLen]，还原系数写入位置
 * @param byteVal 待拆分的字节值（已从 GM/UB 读出并转为 int32）
 * @param pairIdx 字节对序号（0..kPolyLen/2-1），对应输出系数下标 2*pairIdx / 2*pairIdx+1
 */
__aicore__ inline void unpack_d4_pair(AscendC::LocalTensor<int32_t> &out, int32_t byteVal, uint32_t pairIdx)
{
    const int32_t lo = byteVal & 0xF;
    const int32_t hi = (byteVal >> 4) & 0xF;
    out.SetValue(static_cast<int32_t>(2U * pairIdx), lo);
    out.SetValue(static_cast<int32_t>(2U * pairIdx + 1U), hi);
}

#if BYTE_DECODE_D_VEC >= 1

/**
 * d=4 整 poly 解码（VEC=1 默认路径）：偶数位系数（低 4bit）走向量化 widen+mask，
 * 奇数位系数（高 4bit）仍标量右移取出（因右移 4 位后已是「干净」低 4bit，无需再向量掩码）。
 * @param out UB int32[kPolyLen=256]，还原系数输出
 * @param in  UB uint8[kInBytes=128]，输入打包比特流
 * @param tmp UB int32[kPolyLen/2] 中间缓冲：先存 widen 后的字节值，再被 mask_low_bits_i32
 *            原地改写为偶数位系数（低 4bit）
 * @param hi  UB int32[kPolyLen/2] 中间缓冲，供 mask_low_bits_i32 内部移位暂存
 */
__aicore__ inline void poly_byte_decode_d4_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                 AscendC::LocalTensor<int32_t> &tmp,
                                                 AscendC::LocalTensor<int32_t> &hi)
{
    /* kPairs：字节对数 = kPolyLen/2 = 128（每字节拆出 2 个系数）。 */
    constexpr uint32_t kPairs = kPolyLen / 2U;
    /* 步骤1：把 128 个输入字节零扩展为 int32，供下面的向量移位运算使用。 */
    widen_bytes_to_i32(tmp, in, kPairs);
    /* 同步点：确保宽化写入 tmp 完成后，才能对 tmp 做向量掩码。 */
    AscendC::PipeBarrier<PIPE_ALL>();
    /* 步骤2：向量化取低 4bit，得到每字节的偶数位系数（tmp[i] = in[i] & 0xF）。 */
    mask_low_bits_i32(tmp, hi, 4, kPairs);
    /* 同步点：确保掩码写回 tmp 完成后，才能进入下面对 tmp/in 的标量读取。 */
    AscendC::PipeBarrier<PIPE_ALL>();

    /* 步骤3：逐字节把偶数位系数（tmp[i]，已向量掩码好）与奇数位系数（in[i]>>4，标量算）
     * 交替写入输出数组：out[2i]=偶数位，out[2i+1]=奇数位。 */
    for (uint32_t i = 0; i < kPairs; ++i) {
        const int32_t b = static_cast<int32_t>(in.GetValue(i));
        out.SetValue(static_cast<int32_t>(2U * i), tmp.GetValue(static_cast<int32_t>(i)));
        out.SetValue(static_cast<int32_t>(2U * i + 1U), (b >> 4) & 0xF);
    }
}

#else

/**
 * d=4 整 poly 解码（VEC=0 纯标量路径）：逐字节调用 unpack_d4_pair 标量拆分，无向量化。
 * @param out UB int32[kPolyLen=256]，还原系数输出
 * @param in  UB uint8[kInBytes=128]，输入打包比特流
 */
__aicore__ inline void poly_byte_decode_d4_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                 AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t i = 0; i < kPolyLen / 2U; ++i) {
        unpack_d4_pair(out, static_cast<int32_t>(in.GetValue(i)), i);
    }
}

#endif

#elif F203_BYTE_DECODE_D == 5

/**
 * 5B/组 → 8×5bit 系数（Alg.6 逆，与 encode d=5 对称）：每个系数由 1~2 个相邻输入字节
 * 的比特片段拼接（右移取本字节残留高位 + 左移拼下一字节的低位）。
 * @param out   [out] UB int32[kPolyLen]，还原系数写入位置
 * @param in    UB uint8[kInBytes]，输入打包比特流
 * @param group 组号（0..kPolyLen/8-1），本组对应输入字节 [group*5, group*5+5)，
 *              输出系数 [group*8, group*8+8)
 */
__aicore__ inline void unpack_d5_group(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                        uint32_t group)
{
    /* byteBase：本组在 in 中的起始字节偏移；一次性取出本组 5 个字节，避免重复 GetValue。 */
    const uint32_t byteBase = group * 5U;
    const uint8_t b0 = in.GetValue(byteBase + 0U);
    const uint8_t b1 = in.GetValue(byteBase + 1U);
    const uint8_t b2 = in.GetValue(byteBase + 2U);
    const uint8_t b3 = in.GetValue(byteBase + 3U);
    const uint8_t b4 = in.GetValue(byteBase + 4U);
    /* coeffBase：本组在 out 中的起始系数下标；还原规则与 encode 侧 pack_d5_group 严格对称。 */
    const uint32_t coeffBase = group * 8U;
    out.SetValue(static_cast<int32_t>(coeffBase + 0U), static_cast<int32_t>(0x1Fu & (b0 >> 0)));
    out.SetValue(static_cast<int32_t>(coeffBase + 1U), static_cast<int32_t>(0x1Fu & ((b0 >> 5) | (b1 << 3))));
    out.SetValue(static_cast<int32_t>(coeffBase + 2U), static_cast<int32_t>(0x1Fu & (b1 >> 2)));
    out.SetValue(static_cast<int32_t>(coeffBase + 3U), static_cast<int32_t>(0x1Fu & ((b1 >> 7) | (b2 << 1))));
    out.SetValue(static_cast<int32_t>(coeffBase + 4U), static_cast<int32_t>(0x1Fu & ((b2 >> 4) | (b3 << 4))));
    out.SetValue(static_cast<int32_t>(coeffBase + 5U), static_cast<int32_t>(0x1Fu & (b3 >> 1)));
    out.SetValue(static_cast<int32_t>(coeffBase + 6U), static_cast<int32_t>(0x1Fu & ((b3 >> 6) | (b4 << 2))));
    out.SetValue(static_cast<int32_t>(coeffBase + 7U), static_cast<int32_t>(0x1Fu & (b4 >> 3)));
}

#if BYTE_DECODE_D_VEC >= 1

/**
 * d=5 整 poly 解码（VEC=1，与 VEC=0 同体）：逐组标量 unpack，无额外向量化步骤
 * （d=5 比特跨字节边界不规则，未做向量化，与 ByteEncode_d 侧 VEC=2 实验「更慢不采纳」的
 * 结论一致，见 docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md）。
 * @param out UB int32[kPolyLen=256]，还原系数输出
 * @param in  UB uint8[kInBytes=160]，输入打包比特流
 */
__aicore__ inline void poly_byte_decode_d5_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                 AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    /* kPolyLen/8=32 组，每组还原 8 个系数，共 256 个。 */
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        unpack_d5_group(out, in, g);
    }
}

#else

/** d=5 整 poly 解码（VEC=0）：与上面 VEC=1 分支代码体完全相同（同体，无额外向量化差异）。 */
__aicore__ inline void poly_byte_decode_d5_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                 AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        unpack_d5_group(out, in, g);
    }
}

#endif

#elif F203_BYTE_DECODE_D == 10

/**
 * 5B/组 → 4×10bit 系数（与 encode d=10 对称）。
 * @param out   [out] UB int32[kPolyLen]，还原系数写入位置
 * @param in    UB uint8[kInBytes]，输入打包比特流
 * @param group 组号（0..kPolyLen/4-1），本组对应输入字节 [group*5, group*5+5)，
 *              输出系数 [group*4, group*4+4)
 */
__aicore__ inline void unpack_d10_group(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                        uint32_t group)
{
    /* byteBase：本组在 in 中的起始字节偏移；一次性取出本组 5 个字节。 */
    const uint32_t byteBase = group * 5U;
    const uint8_t b0 = in.GetValue(byteBase + 0U);
    const uint8_t b1 = in.GetValue(byteBase + 1U);
    const uint8_t b2 = in.GetValue(byteBase + 2U);
    const uint8_t b3 = in.GetValue(byteBase + 3U);
    const uint8_t b4 = in.GetValue(byteBase + 4U);
    /* t0..t3：还原出的 4 个 10bit 系数，与 encode 侧 pack_d10_group 严格对称。 */
    const uint16_t t0 = static_cast<uint16_t>(0x3FFu & ((b0 >> 0) | ((uint16_t)b1 << 8)));
    const uint16_t t1 = static_cast<uint16_t>(0x3FFu & ((b1 >> 2) | ((uint16_t)b2 << 6)));
    const uint16_t t2 = static_cast<uint16_t>(0x3FFu & ((b2 >> 4) | ((uint16_t)b3 << 4)));
    const uint16_t t3 = static_cast<uint16_t>(0x3FFu & ((b3 >> 6) | ((uint16_t)b4 << 2)));
    const uint32_t coeffBase = group * 4U;
    out.SetValue(static_cast<int32_t>(coeffBase + 0U), static_cast<int32_t>(t0));
    out.SetValue(static_cast<int32_t>(coeffBase + 1U), static_cast<int32_t>(t1));
    out.SetValue(static_cast<int32_t>(coeffBase + 2U), static_cast<int32_t>(t2));
    out.SetValue(static_cast<int32_t>(coeffBase + 3U), static_cast<int32_t>(t3));
}

#if BYTE_DECODE_D_VEC >= 1

/**
 * d=10 整 poly 解码（VEC=1，与 VEC=0 同体）：逐组标量 unpack，无额外向量化步骤。
 * @param out UB int32[kPolyLen=256]，还原系数输出
 * @param in  UB uint8[kInBytes=320]，输入打包比特流
 */
__aicore__ inline void poly_byte_decode_d10_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                  AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    /* kPolyLen/4=64 组，每组还原 4 个系数，共 256 个。 */
    for (uint32_t g = 0; g < kPolyLen / 4U; ++g) {
        unpack_d10_group(out, in, g);
    }
}

#else

/** d=10 整 poly 解码（VEC=0）：与上面 VEC=1 分支代码体完全相同（同体）。 */
__aicore__ inline void poly_byte_decode_d10_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                  AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 4U; ++g) {
        unpack_d10_group(out, in, g);
    }
}

#endif

#elif F203_BYTE_DECODE_D == 11

/**
 * 11B/组 → 8×11bit 系数（ML-KEM-1024 c₁，与 encode d=11 对称）。
 * @param out   [out] UB int32[kPolyLen]，还原系数写入位置
 * @param in    UB uint8[kInBytes]，输入打包比特流
 * @param group 组号（0..kPolyLen/8-1），本组对应输入字节 [group*11, group*11+11)，
 *              输出系数 [group*8, group*8+8)
 */
__aicore__ inline void unpack_d11_group(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                        uint32_t group)
{
    /* byteBase：本组在 in 中的起始字节偏移；一次性取出本组 11 个字节。 */
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
    /* t0..t7：还原出的 8 个 11bit 系数，与 encode 侧 pack_d11_group 严格对称。 */
    const uint16_t t0 = static_cast<uint16_t>(0x7FFu & ((b0 >> 0) | ((uint16_t)b1 << 8)));
    const uint16_t t1 = static_cast<uint16_t>(0x7FFu & ((b1 >> 3) | ((uint16_t)b2 << 5)));
    const uint16_t t2 = static_cast<uint16_t>(0x7FFu & ((b2 >> 6) | ((uint16_t)b3 << 2) | ((uint16_t)b4 << 10)));
    const uint16_t t3 = static_cast<uint16_t>(0x7FFu & ((b4 >> 1) | ((uint16_t)b5 << 7)));
    const uint16_t t4 = static_cast<uint16_t>(0x7FFu & ((b5 >> 4) | ((uint16_t)b6 << 4)));
    const uint16_t t5 = static_cast<uint16_t>(0x7FFu & ((b6 >> 7) | ((uint16_t)b7 << 1) | ((uint16_t)b8 << 9)));
    const uint16_t t6 = static_cast<uint16_t>(0x7FFu & ((b8 >> 2) | ((uint16_t)b9 << 6)));
    const uint16_t t7 = static_cast<uint16_t>(0x7FFu & ((b9 >> 5) | ((uint16_t)b10 << 3)));
    const uint32_t coeffBase = group * 8U;
    out.SetValue(static_cast<int32_t>(coeffBase + 0U), static_cast<int32_t>(t0));
    out.SetValue(static_cast<int32_t>(coeffBase + 1U), static_cast<int32_t>(t1));
    out.SetValue(static_cast<int32_t>(coeffBase + 2U), static_cast<int32_t>(t2));
    out.SetValue(static_cast<int32_t>(coeffBase + 3U), static_cast<int32_t>(t3));
    out.SetValue(static_cast<int32_t>(coeffBase + 4U), static_cast<int32_t>(t4));
    out.SetValue(static_cast<int32_t>(coeffBase + 5U), static_cast<int32_t>(t5));
    out.SetValue(static_cast<int32_t>(coeffBase + 6U), static_cast<int32_t>(t6));
    out.SetValue(static_cast<int32_t>(coeffBase + 7U), static_cast<int32_t>(t7));
}

#if BYTE_DECODE_D_VEC >= 1

/**
 * d=11 整 poly 解码（VEC=1，与 VEC=0 同体）：逐组标量 unpack，无额外向量化步骤。
 * @param out UB int32[kPolyLen=256]，还原系数输出
 * @param in  UB uint8[kInBytes=352]，输入打包比特流
 */
__aicore__ inline void poly_byte_decode_d11_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                  AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    /* kPolyLen/8=32 组，每组还原 8 个系数，共 256 个。 */
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        unpack_d11_group(out, in, g);
    }
}

#else

/** d=11 整 poly 解码（VEC=0）：与上面 VEC=1 分支代码体完全相同（同体）。 */
__aicore__ inline void poly_byte_decode_d11_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                                  AscendC::LocalTensor<int32_t> &, AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        unpack_d11_group(out, in, g);
    }
}

#endif

#endif

/**
 * ByteDecode_d 统一入口：按编译期 F203_BYTE_DECODE_D（4/5/10/11）分发到对应实现
 * （本函数体全部为编译期 #if 分支，无运行期分支开销）。
 * 由 byte_decode_d_custom.cpp 的 kernel 调用，是本文件对外的唯一入口函数。
 * @param out UB int32[kPolyLen]，还原系数输出
 * @param in  UB uint8[kInBytes]，输入打包比特流（长度随 d 变化，见 config 头）
 * @param tmp UB int32 中间缓冲：仅 d=4 向量路径使用，存放 widen 后再被掩码的偶数位系数
 * @param hi  UB int32 中间缓冲：仅 d=4 向量路径使用，供 mask_low_bits_i32 内部移位暂存
 */
__aicore__ inline void poly_byte_decode_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<uint8_t> &in,
                                              AscendC::LocalTensor<int32_t> &tmp, AscendC::LocalTensor<int32_t> &hi)
{
#if F203_BYTE_DECODE_D == 4
    poly_byte_decode_d4_local(out, in, tmp, hi);
#elif F203_BYTE_DECODE_D == 5
    poly_byte_decode_d5_local(out, in, tmp, hi);
#elif F203_BYTE_DECODE_D == 10
    poly_byte_decode_d10_local(out, in, tmp, hi);
#elif F203_BYTE_DECODE_D == 11
    poly_byte_decode_d11_local(out, in, tmp, hi);
#endif
}

}  // namespace byte_decode_d

#endif
