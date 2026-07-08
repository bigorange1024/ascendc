#ifndef BYTE_ENCODE_D_VEC_HPP
#define BYTE_ENCODE_D_VEC_HPP

/**
 * @file byte_encode_d_vec.hpp
 * @brief FIPS 203 Alg.5 ByteEncode_d（d=4/5/10/11）。
 *
 * 宏分层（详见 docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md）：
 *   BYTE_ENCODE_D_VEC=0 — 纯标量 pack
 *   BYTE_ENCODE_D_VEC=1 — 默认验收：向量 mask_low_bits + 标量逐组 pack（d=5/11 生产基线）
 *   BYTE_ENCODE_D_VEC=2 — 真·向量 pack（Gather+byte-lane；**保留代码、默认不激活**；d=5/11 实验更慢）
 * d=12 真·向量见 pass-fix-f203-2s1e-byteencode12-vec-k4（2×12bit=3B 对齐，与 d=5/11 不同类问题）。
 */
#include "byte_encode_d_config.hpp"
#include "f203_mlkem_params.h"
#include "kernel_operator.h"

namespace byte_encode_d {

constexpr uint32_t kPolyLen = static_cast<uint32_t>(F203_MLKEM_N);
constexpr uint32_t kOutBytes = static_cast<uint32_t>(F203_BYTE_ENCODE_POLY_BYTES);

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

#if BYTE_ENCODE_D_VEC >= 2
/*
 * ── 真·向量 pack（BYTE_ENCODE_D_VEC=2；仅 d=5 / d=11）────────────────────────────────
 * 背景：d=5(8 系数=5B)、d=11(8 系数=11B) 的比特拼装跨字节边界不规则，历史 VEC=1 只把
 *       低位掩码向量化、拼字节仍逐组标量。此档仿 byte_encode12_vec.hpp 的 d=12 方案，
 *       把「取系数 + 算 byte-lane」全做成向量：
 *         1) Gather 把每组第 k 个系数(k=0..7)聚成一条 32-lane 向量 t_k（32 组=poly）；
 *         2) 每个输出 byte-lane b_i = Σ(t·2^shift 或 t>>shift) 用 Muls/ShiftRight/Add 向量算；
 *         3) 每 4 组拼成整 int32 字（拼字仍用标量 GetValue，与 d=12 一致），再批量 DataCopy。
 *       byte-lane 拼装靠加法（各 t 的比特区间互不重叠，加法≡或），最终 &0xFF 在拼字时统一取低字节。
 * 结论：byte-lane 算术从「每系数标量移位」降为「32-lane 向量」；拼字/搬出为 O(N/4) 整字。
 * 未采用：把拼字也做成向量——单字节 stride 的散写无合适 intrinsic，故沿用 d=12 的标量拼字。
 */
constexpr uint32_t kVpGroups = kPolyLen / 8U;   // 每 poly 32 组，每组 8 系数

// scratch(int32,≥792) 分区：off[0,32) idxByte[32,64) t0..t7[64,320) b0..b10[320,672)
//                            tmp[672,704) packW[704,792)
constexpr uint32_t kVpOffScratch = 0U;
constexpr uint32_t kVpIdxByte = 32U;
constexpr uint32_t kVpTBase = 64U;              // t_k @ kVpTBase + k*32
constexpr uint32_t kVpBBase = 320U;             // b_i @ kVpBBase + i*32
constexpr uint32_t kVpTmp = 672U;
constexpr uint32_t kVpPackW = 704U;

/** 取整字第 lane 个 int32 的低字节（0..255），用于拼装输出整字。 */
__aicore__ inline int32_t vp_byte(AscendC::LocalTensor<int32_t> &t, int32_t lane)
{
    return static_cast<int32_t>(t.GetValue(lane) & 0xFF);
}

/**
 * 共用：建组基偏移向量 idxByte[gg]=gg*32 字节，并 Gather 全部 8 个 position-lane。
 * t_k[gg] = in[gg*8 + k]；字节偏移 = gg*32 + k*4（每组 8 int32 = 32B）。
 * 前置条件：in 为 Compress_d 输出，各系数已 < 2^d（无脏高位），故此处**不再掩码**，
 *           byte-lane 的比特截断统一由拼字阶段 &0xFF 完成（省 8×mask + barrier）。
 * 逐组内第 k 系数复用同一 off 缓冲，故 Gather(k) 与下一轮 Adds(k+1) 间需 barrier。
 */
__aicore__ inline void vp_gather_all8(AscendC::LocalTensor<int32_t> &scratch, AscendC::LocalTensor<int32_t> &in,
                                      AscendC::LocalTensor<int32_t> (&t)[8])
{
    using AscendC::Adds;
    using AscendC::CreateVecIndex;
    using AscendC::Gather;
    using AscendC::Muls;
    const int32_t g = static_cast<int32_t>(kVpGroups);
    AscendC::LocalTensor<int32_t> off = scratch[kVpOffScratch];
    AscendC::LocalTensor<int32_t> idxByte = scratch[kVpIdxByte];
    CreateVecIndex(off, static_cast<int32_t>(0), kVpGroups);
    Muls(idxByte, off, static_cast<int32_t>(32), g);   // gg*32 字节
    AscendC::PipeBarrier<PIPE_ALL>();
    for (int32_t k = 0; k < 8; ++k) {
        t[k] = scratch[kVpTBase + static_cast<uint32_t>(k) * 32U];
        Adds(off, idxByte, k * 4, g);
        AscendC::PipeBarrier<PIPE_ALL>();
        Gather(t[k], in, off.ReinterpretCast<uint32_t>(), 0U, kVpGroups);
        AscendC::PipeBarrier<PIPE_ALL>();
    }
}
#endif  // BYTE_ENCODE_D_VEC >= 2

#if F203_BYTE_ENCODE_D == 4

__aicore__ inline void pack_d4_group(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                     uint32_t group)
{
    const uint32_t base = group * 8U;
    const uint8_t t0 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 0U)) & 0xF);
    const uint8_t t1 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 1U)) & 0xF);
    const uint8_t t2 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 2U)) & 0xF);
    const uint8_t t3 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 3U)) & 0xF);
    const uint8_t t4 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 4U)) & 0xF);
    const uint8_t t5 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 5U)) & 0xF);
    const uint8_t t6 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 6U)) & 0xF);
    const uint8_t t7 = static_cast<uint8_t>(in.GetValue(static_cast<int32_t>(base + 7U)) & 0xF);
    const uint32_t byteBase = group * 4U;
    out.SetValue(byteBase + 0U, static_cast<uint8_t>(t0 | (t1 << 4)));
    out.SetValue(byteBase + 1U, static_cast<uint8_t>(t2 | (t3 << 4)));
    out.SetValue(byteBase + 2U, static_cast<uint8_t>(t4 | (t5 << 4)));
    out.SetValue(byteBase + 3U, static_cast<uint8_t>(t6 | (t7 << 4)));
}

#if BYTE_ENCODE_D_VEC >= 1

__aicore__ inline void poly_byte_encode_d4_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                                 AscendC::LocalTensor<int32_t> &tmp)
{
    mask_low_bits_i32(in, tmp, 4, kPolyLen);
    AscendC::PipeBarrier<PIPE_ALL>();
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        pack_d4_group(out, in, g);
    }
}

#else

__aicore__ inline void poly_byte_encode_d4_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                                 AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        pack_d4_group(out, in, g);
    }
}

#endif

#elif F203_BYTE_ENCODE_D == 5

/** 8 系数 × 5bit → 5B；与 ml-kem-native poly_compress_d5 比特布局一致。 */
__aicore__ inline void pack_d5_group(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                     uint32_t group)
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

#if BYTE_ENCODE_D_VEC >= 1

__aicore__ inline void poly_byte_encode_d5_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                                 AscendC::LocalTensor<int32_t> &tmp)
{
    mask_low_bits_i32(in, tmp, 5, kPolyLen);
    AscendC::PipeBarrier<PIPE_ALL>();
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        pack_d5_group(out, in, g);
    }
}

#else

__aicore__ inline void poly_byte_encode_d5_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                                 AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        pack_d5_group(out, in, g);
    }
}

#endif

#if BYTE_ENCODE_D_VEC >= 2
/**
 * d=5 真·向量 pack：256 系数 → 160B（32 组 × 5B）。
 * @param in      UB int32[256]，Compress₅ 输出（假定 <2^5，内部再掩一次防脏高位）。
 * @param scratch UB int32(≥792)，见 vp_* 分区常量。
 * byte-lane（组内 8 系数 t0..t7，每 5bit）：
 *   b0 = t0 | t1<<5 ; b1 = t1>>3 | t2<<2 | t3<<7 ; b2 = t3>>1 | t4<<4 ;
 *   b3 = t4>>4 | t5<<1 | t6<<6 ; b4 = t6>>2 | t7<<3（拼字时 &0xFF 取低字节）。
 */
__aicore__ inline void poly_byte_encode_d5_vecpack(AscendC::LocalTensor<uint8_t> &out,
                                                   AscendC::LocalTensor<int32_t> &in,
                                                   AscendC::LocalTensor<int32_t> &scratch)
{
    using AscendC::Add;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    const int32_t g = static_cast<int32_t>(kVpGroups);

    AscendC::LocalTensor<int32_t> t[8];
    vp_gather_all8(scratch, in, t);

    AscendC::LocalTensor<int32_t> tmp = scratch[kVpTmp];
    AscendC::LocalTensor<int32_t> b[5];
    for (int32_t i = 0; i < 5; ++i) {
        b[i] = scratch[kVpBBase + static_cast<uint32_t>(i) * 32U];
    }

    // b0 = t0 + t1*32
    Muls(tmp, t[1], static_cast<int32_t>(32), g);
    Add(b[0], t[0], tmp, g);
    // b1 = (t1>>3) + t2*4 + t3*128
    ShiftRight(b[1], t[1], 3, g);
    Muls(tmp, t[2], static_cast<int32_t>(4), g);
    Add(b[1], b[1], tmp, g);
    Muls(tmp, t[3], static_cast<int32_t>(128), g);
    Add(b[1], b[1], tmp, g);
    // b2 = (t3>>1) + t4*16
    ShiftRight(b[2], t[3], 1, g);
    Muls(tmp, t[4], static_cast<int32_t>(16), g);
    Add(b[2], b[2], tmp, g);
    // b3 = (t4>>4) + t5*2 + t6*64
    ShiftRight(b[3], t[4], 4, g);
    Muls(tmp, t[5], static_cast<int32_t>(2), g);
    Add(b[3], b[3], tmp, g);
    Muls(tmp, t[6], static_cast<int32_t>(64), g);
    Add(b[3], b[3], tmp, g);
    // b4 = (t6>>2) + t7*8
    ShiftRight(b[4], t[6], 2, g);
    Muls(tmp, t[7], static_cast<int32_t>(8), g);
    Add(b[4], b[4], tmp, g);
    AscendC::PipeBarrier<PIPE_ALL>();

    // 每 4 组(p..p+3) → 20B = 5 int32 字；32 组 → 8 轮 × 5 字 = 40 字 = 160B。
    AscendC::LocalTensor<int32_t> packW = scratch[kVpPackW];
    for (int32_t q = 0; q < static_cast<int32_t>(kVpGroups) / 4; ++q) {
        const int32_t p = q * 4;
        const int32_t w = q * 5;
        packW.SetValue(w + 0, vp_byte(b[0], p) | (vp_byte(b[1], p) << 8) | (vp_byte(b[2], p) << 16) |
                                  (vp_byte(b[3], p) << 24));
        packW.SetValue(w + 1, vp_byte(b[4], p) | (vp_byte(b[0], p + 1) << 8) | (vp_byte(b[1], p + 1) << 16) |
                                  (vp_byte(b[2], p + 1) << 24));
        packW.SetValue(w + 2, vp_byte(b[3], p + 1) | (vp_byte(b[4], p + 1) << 8) | (vp_byte(b[0], p + 2) << 16) |
                                  (vp_byte(b[1], p + 2) << 24));
        packW.SetValue(w + 3, vp_byte(b[2], p + 2) | (vp_byte(b[3], p + 2) << 8) | (vp_byte(b[4], p + 2) << 16) |
                                  (vp_byte(b[0], p + 3) << 24));
        packW.SetValue(w + 4, vp_byte(b[1], p + 3) | (vp_byte(b[2], p + 3) << 8) | (vp_byte(b[3], p + 3) << 16) |
                                  (vp_byte(b[4], p + 3) << 24));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(out, packW.ReinterpretCast<uint8_t>(), kPolyLen / 8U * 5U);
    AscendC::PipeBarrier<PIPE_ALL>();
}
#endif

#elif F203_BYTE_ENCODE_D == 10

__aicore__ inline void pack_d10_group(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                      uint32_t group)
{
    const uint32_t base = group * 4U;
    const uint16_t t0 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 0U)) & 0x3FF);
    const uint16_t t1 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 1U)) & 0x3FF);
    const uint16_t t2 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 2U)) & 0x3FF);
    const uint16_t t3 = static_cast<uint16_t>(in.GetValue(static_cast<int32_t>(base + 3U)) & 0x3FF);
    const uint32_t byteBase = group * 5U;
    out.SetValue(byteBase + 0U, static_cast<uint8_t>((t0 >> 0) & 0xFF));
    out.SetValue(byteBase + 1U, static_cast<uint8_t>((t0 >> 8) | ((t1 << 2) & 0xFF)));
    out.SetValue(byteBase + 2U, static_cast<uint8_t>((t1 >> 6) | ((t2 << 4) & 0xFF)));
    out.SetValue(byteBase + 3U, static_cast<uint8_t>((t2 >> 4) | ((t3 << 6) & 0xFF)));
    out.SetValue(byteBase + 4U, static_cast<uint8_t>(t3 >> 2));
}

#if BYTE_ENCODE_D_VEC >= 1

__aicore__ inline void poly_byte_encode_d10_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                                  AscendC::LocalTensor<int32_t> &tmp)
{
    mask_low_bits_i32(in, tmp, 10, kPolyLen);
    AscendC::PipeBarrier<PIPE_ALL>();
    for (uint32_t g = 0; g < kPolyLen / 4U; ++g) {
        pack_d10_group(out, in, g);
    }
}

#else

__aicore__ inline void poly_byte_encode_d10_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                                  AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 4U; ++g) {
        pack_d10_group(out, in, g);
    }
}

#endif

#elif F203_BYTE_ENCODE_D == 11

/** 8 系数 × 11bit → 11B；ML-KEM-1024 c₁ 单 poly 352B。 */
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

#if BYTE_ENCODE_D_VEC >= 1

__aicore__ inline void poly_byte_encode_d11_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                                  AscendC::LocalTensor<int32_t> &tmp)
{
    mask_low_bits_i32(in, tmp, 11, kPolyLen);
    AscendC::PipeBarrier<PIPE_ALL>();
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        pack_d11_group(out, in, g);
    }
}

#else

__aicore__ inline void poly_byte_encode_d11_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                                  AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t g = 0; g < kPolyLen / 8U; ++g) {
        pack_d11_group(out, in, g);
    }
}

#endif

#if BYTE_ENCODE_D_VEC >= 2
/**
 * d=11 真·向量 pack：256 系数 → 352B（32 组 × 11B）。
 * @param in      UB int32[256]，Compress₁₁ 输出（假定 <2^11，内部再掩一次防脏高位）。
 * @param scratch UB int32(≥792)，见 vp_* 分区常量。
 * byte-lane（组内 8 系数 t0..t7，每 11bit）：
 *   b0=t0 ; b1=t0>>8|t1<<3 ; b2=t1>>5|t2<<6 ; b3=t2>>2 ; b4=t2>>10|t3<<1 ;
 *   b5=t3>>7|t4<<4 ; b6=t4>>4|t5<<7 ; b7=t5>>1 ; b8=t5>>9|t6<<2 ; b9=t6>>6|t7<<5 ; b10=t7>>3
 *   （拼字时 &0xFF 取低字节）。
 */
__aicore__ inline void poly_byte_encode_d11_vecpack(AscendC::LocalTensor<uint8_t> &out,
                                                    AscendC::LocalTensor<int32_t> &in,
                                                    AscendC::LocalTensor<int32_t> &scratch)
{
    using AscendC::Add;
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    const int32_t g = static_cast<int32_t>(kVpGroups);

    AscendC::LocalTensor<int32_t> t[8];
    vp_gather_all8(scratch, in, t);

    AscendC::LocalTensor<int32_t> tmp = scratch[kVpTmp];
    AscendC::LocalTensor<int32_t> b[11];
    for (int32_t i = 0; i < 11; ++i) {
        b[i] = scratch[kVpBBase + static_cast<uint32_t>(i) * 32U];
    }

    Adds(b[0], t[0], 0, g);                                    // b0 = t0（低字节由拼字 &0xFF 取）
    ShiftRight(b[1], t[0], 8, g);                              // b1 = (t0>>8) + t1*8
    Muls(tmp, t[1], static_cast<int32_t>(8), g);
    Add(b[1], b[1], tmp, g);
    ShiftRight(b[2], t[1], 5, g);                              // b2 = (t1>>5) + t2*64
    Muls(tmp, t[2], static_cast<int32_t>(64), g);
    Add(b[2], b[2], tmp, g);
    ShiftRight(b[3], t[2], 2, g);                              // b3 = t2>>2
    ShiftRight(b[4], t[2], 10, g);                             // b4 = (t2>>10) + t3*2
    Muls(tmp, t[3], static_cast<int32_t>(2), g);
    Add(b[4], b[4], tmp, g);
    ShiftRight(b[5], t[3], 7, g);                              // b5 = (t3>>7) + t4*16
    Muls(tmp, t[4], static_cast<int32_t>(16), g);
    Add(b[5], b[5], tmp, g);
    ShiftRight(b[6], t[4], 4, g);                              // b6 = (t4>>4) + t5*128
    Muls(tmp, t[5], static_cast<int32_t>(128), g);
    Add(b[6], b[6], tmp, g);
    ShiftRight(b[7], t[5], 1, g);                              // b7 = t5>>1
    ShiftRight(b[8], t[5], 9, g);                              // b8 = (t5>>9) + t6*4
    Muls(tmp, t[6], static_cast<int32_t>(4), g);
    Add(b[8], b[8], tmp, g);
    ShiftRight(b[9], t[6], 6, g);                              // b9 = (t6>>6) + t7*32
    Muls(tmp, t[7], static_cast<int32_t>(32), g);
    Add(b[9], b[9], tmp, g);
    ShiftRight(b[10], t[7], 3, g);                             // b10 = t7>>3
    AscendC::PipeBarrier<PIPE_ALL>();

    // 每 4 组(p..p+3) → 44B = 11 int32 字；32 组 → 8 轮 × 11 字 = 88 字 = 352B。
    AscendC::LocalTensor<int32_t> packW = scratch[kVpPackW];
    for (int32_t q = 0; q < static_cast<int32_t>(kVpGroups) / 4; ++q) {
        const int32_t p = q * 4;
        const int32_t w = q * 11;
        packW.SetValue(w + 0, vp_byte(b[0], p) | (vp_byte(b[1], p) << 8) | (vp_byte(b[2], p) << 16) |
                                  (vp_byte(b[3], p) << 24));
        packW.SetValue(w + 1, vp_byte(b[4], p) | (vp_byte(b[5], p) << 8) | (vp_byte(b[6], p) << 16) |
                                  (vp_byte(b[7], p) << 24));
        packW.SetValue(w + 2, vp_byte(b[8], p) | (vp_byte(b[9], p) << 8) | (vp_byte(b[10], p) << 16) |
                                  (vp_byte(b[0], p + 1) << 24));
        packW.SetValue(w + 3, vp_byte(b[1], p + 1) | (vp_byte(b[2], p + 1) << 8) | (vp_byte(b[3], p + 1) << 16) |
                                  (vp_byte(b[4], p + 1) << 24));
        packW.SetValue(w + 4, vp_byte(b[5], p + 1) | (vp_byte(b[6], p + 1) << 8) | (vp_byte(b[7], p + 1) << 16) |
                                  (vp_byte(b[8], p + 1) << 24));
        packW.SetValue(w + 5, vp_byte(b[9], p + 1) | (vp_byte(b[10], p + 1) << 8) | (vp_byte(b[0], p + 2) << 16) |
                                  (vp_byte(b[1], p + 2) << 24));
        packW.SetValue(w + 6, vp_byte(b[2], p + 2) | (vp_byte(b[3], p + 2) << 8) | (vp_byte(b[4], p + 2) << 16) |
                                  (vp_byte(b[5], p + 2) << 24));
        packW.SetValue(w + 7, vp_byte(b[6], p + 2) | (vp_byte(b[7], p + 2) << 8) | (vp_byte(b[8], p + 2) << 16) |
                                  (vp_byte(b[9], p + 2) << 24));
        packW.SetValue(w + 8, vp_byte(b[10], p + 2) | (vp_byte(b[0], p + 3) << 8) | (vp_byte(b[1], p + 3) << 16) |
                                  (vp_byte(b[2], p + 3) << 24));
        packW.SetValue(w + 9, vp_byte(b[3], p + 3) | (vp_byte(b[4], p + 3) << 8) | (vp_byte(b[5], p + 3) << 16) |
                                  (vp_byte(b[6], p + 3) << 24));
        packW.SetValue(w + 10, vp_byte(b[7], p + 3) | (vp_byte(b[8], p + 3) << 8) | (vp_byte(b[9], p + 3) << 16) |
                                   (vp_byte(b[10], p + 3) << 24));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(out, packW.ReinterpretCast<uint8_t>(), kPolyLen / 8U * 11U);
    AscendC::PipeBarrier<PIPE_ALL>();
}
#endif

#endif

__aicore__ inline void poly_byte_encode_local(AscendC::LocalTensor<uint8_t> &out, AscendC::LocalTensor<int32_t> &in,
                                              AscendC::LocalTensor<int32_t> &tmp)
{
#if F203_BYTE_ENCODE_D == 4
    poly_byte_encode_d4_local(out, in, tmp);
#elif F203_BYTE_ENCODE_D == 5
#if BYTE_ENCODE_D_VEC >= 2
    poly_byte_encode_d5_vecpack(out, in, tmp);
#else
    poly_byte_encode_d5_local(out, in, tmp);
#endif
#elif F203_BYTE_ENCODE_D == 10
    poly_byte_encode_d10_local(out, in, tmp);
#elif F203_BYTE_ENCODE_D == 11
#if BYTE_ENCODE_D_VEC >= 2
    poly_byte_encode_d11_vecpack(out, in, tmp);
#else
    poly_byte_encode_d11_local(out, in, tmp);
#endif
#endif
}

}  // namespace byte_encode_d

#endif
