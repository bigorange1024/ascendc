/**
 * @file f203_decrypt_tail_compress1_byteencode1.hpp
 * @brief Alg.15 行 6–7 尾：w←(v'−w_time) mod q → Compress₁ → ByteEncode₁ → m。
 *
 * 除 ByteEncode₁ 外全向量。v−w：Sub + wrap_mod。
 * Compress₁：liboqs Barrett 1290168，向量写回 wCan（原地）。
 * Encode₁：LSB-first 标量 pack（唯一标量段）；与 Compress **融合**——不另开 bits[256] 中间 UB。
 *
 * 背景：分层实现曾用独立 bits 缓冲再 GetValue；融合后少 1×256 int32 UB，语义不变。
 * 未采用：Gather/GatherMask 真·向量 bit 流（ROI 差；k3 密文 d=4/10，尾段 d=1 仍走标量 pack）。
 */
#ifndef F203_DECRYPT_TAIL_COMPRESS1_BYTEENCODE1_HPP
#define F203_DECRYPT_TAIL_COMPRESS1_BYTEENCODE1_HPP

#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

#ifndef COMPRESS_1_VEC
#define COMPRESS_1_VEC 1
#endif

namespace decrypt_device {

constexpr int32_t kTailN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kTailQ = static_cast<int32_t>(F203_DECRYPT_Q);
constexpr int32_t kCompress1Mul = 1290168;
constexpr int32_t kCompress1Bias = static_cast<int32_t>(1U << 30);
constexpr int32_t kCompress1Shift = 31;

__aicore__ inline uint32_t compress_1_barrett_u32(uint32_t u)
{
    const uint32_t d0 = u * 1290168u;
    return (d0 + (1u << 30)) >> 31;
}

/**
 * 将已约化到大致 [-q,q) 的差约化到 [0,q)。
 * 与 hat_ip::wrap_mod_vec_runtime 同构。
 * 前置：输入为 v−w_time（系数已在 [0,q)，差 ∈ (-q,q)）。
 */
__aicore__ inline void wrap_mod_q_vec(AscendC::LocalTensor<int32_t> &dst, AscendC::LocalTensor<int32_t> &src,
                                      AscendC::LocalTensor<int32_t> &t1, AscendC::LocalTensor<int32_t> &t2)
{
    using AscendC::Adds;
    using AscendC::Max;
    using AscendC::Mul;
    using AscendC::ShiftRight;
    Adds(t1, src, -kTailQ, kTailN);
    auto &t1_u32 = *reinterpret_cast<AscendC::LocalTensor<uint32_t> *>(&t1);
    auto &t2_u32 = *reinterpret_cast<AscendC::LocalTensor<uint32_t> *>(&t2);
    ShiftRight(t2_u32, t1_u32, 31U, kTailN);
    Mul(t2, src, t2, kTailN);
    Max(dst, t1, t2, kTailN);
}

/** w = (v − w_time) mod q，全向量。 */
__aicore__ inline void sub_mod_q_vec(AscendC::LocalTensor<int32_t> &wOut, AscendC::LocalTensor<int32_t> &vIn,
                                     AscendC::LocalTensor<int32_t> &wTimeIn, AscendC::LocalTensor<int32_t> &t1,
                                     AscendC::LocalTensor<int32_t> &t2)
{
    using AscendC::Sub;
    Sub(wOut, vIn, wTimeIn, kTailN);
    AscendC::PipeBarrier<PIPE_V>();
    wrap_mod_q_vec(wOut, wOut, t1, t2);
}

/**
 * Compress₁ Barrett，可原地（dst 与 uCan 可为同一 LocalTensor）。
 * 输出每 lane ∈ {0,1}（int32）。
 */
__aicore__ inline void compress_1_barrett_vec(AscendC::LocalTensor<int32_t> &dst,
                                              AscendC::LocalTensor<int32_t> &uCan)
{
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    Muls(dst, uCan, kCompress1Mul, kTailN);
    Adds(dst, dst, kCompress1Bias, kTailN);
    ShiftRight(dst, dst, kCompress1Shift, kTailN);
}

/**
 * Compress₁（向量，原地写 wCan）+ ByteEncode₁（标量 pack）融合。
 * 输入：wCan 为已约化到 [0,q) 的 w；输出后 wCan 被覆盖为 {0,1} bits。
 * Encode：每 8 系数 → 1 字节（LSB-first），写 m[32]。
 */
__aicore__ inline void compress1_byteencode1_fused(AscendC::LocalTensor<int32_t> &wCan, __gm__ uint8_t *mOut)
{
#if COMPRESS_1_VEC >= 1
    compress_1_barrett_vec(wCan, wCan);
#else
    for (int32_t i = 0; i < kTailN; ++i) {
        wCan.SetValue(i, static_cast<int32_t>(compress_1_barrett_u32(static_cast<uint32_t>(wCan.GetValue(i)))));
    }
#endif
    AscendC::PipeBarrier<PIPE_ALL>();

    /* 唯一标量段：按字节组拼装，避免独立 bits[256] 缓冲 */
    for (uint32_t b = 0; b < F203_MSG_BYTES; ++b) {
        uint8_t byte = 0U;
        const int32_t base = static_cast<int32_t>(b) * 8;
        for (int32_t k = 0; k < 8; ++k) {
            const uint32_t bit = static_cast<uint32_t>(wCan.GetValue(base + k)) & 1U;
            byte |= static_cast<uint8_t>(bit << k);
        }
        mOut[b] = byte;
    }
}

/**
 * Alg.15 行 6–7 入口：m ← ByteEncode₁(Compress₁((v'−w) mod q))。
 * @param vGm      v' [N] int32（unpack 产出）
 * @param wTimeGm  w 时域 [N] int32（INTT 产出）
 * @param mGm      输出 m[32]
 * 前置：仅 AIV0；生产唯一 D2H 目标。
 */
__aicore__ inline void extract_m_compress1_byteencode1(GM_ADDR vGm, GM_ADDR wTimeGm, GM_ADDR mGm)
{
    AscendC::GlobalTensor<int32_t> gmV;
    AscendC::GlobalTensor<int32_t> gmW;
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(vGm), static_cast<uint32_t>(kTailN));
    gmW.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(wTimeGm), static_cast<uint32_t>(kTailN));
    auto *mOut = reinterpret_cast<__gm__ uint8_t *>(mGm);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queV;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queW;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufW;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufT1;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufT2;

    const uint32_t bytes = static_cast<uint32_t>(kTailN) * static_cast<uint32_t>(sizeof(int32_t));
    pipe.InitBuffer(queV, 1, bytes);
    pipe.InitBuffer(queW, 1, bytes);
    pipe.InitBuffer(bufW, bytes);
    pipe.InitBuffer(bufT1, bytes);
    pipe.InitBuffer(bufT2, bytes);

    AscendC::LocalTensor<int32_t> vLocal = queV.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> wTimeLocal = queW.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> wCan = bufW.Get<int32_t>();
    AscendC::LocalTensor<int32_t> t1 = bufT1.Get<int32_t>();
    AscendC::LocalTensor<int32_t> t2 = bufT2.Get<int32_t>();

    AscendC::DataCopy(vLocal, gmV, static_cast<uint32_t>(kTailN));
    AscendC::DataCopy(wTimeLocal, gmW, static_cast<uint32_t>(kTailN));
    AscendC::PipeBarrier<PIPE_ALL>();

    /* w ← (v' − w_time) mod q */
    sub_mod_q_vec(wCan, vLocal, wTimeLocal, t1, t2);
    AscendC::PipeBarrier<PIPE_ALL>();

    /* Compress₁ 原地 + Encode₁ → mGm */
    compress1_byteencode1_fused(wCan, mOut);

    queV.FreeTensor(vLocal);
    queW.FreeTensor(wTimeLocal);
}

} // namespace decrypt_device

#endif
