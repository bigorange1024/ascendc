/**
 * @file f203_decrypt_unpack_impl.hpp
 * @brief Alg.15 行 3–4：c → u'/v'。ByteDecode₁₁/₅ **标量** + Decompress₁₁/₅ **向量**。
 *
 * 背景：G4 全标量；本探针 prep 向量化 Decompress（P-DEC）。
 * 结论：Decode 标量；Decompress 默认向量；中间态留 device GM，Host 不 D2H。
 */
#ifndef F203_DECRYPT_UNPACK_IMPL_HPP
#define F203_DECRYPT_UNPACK_IMPL_HPP

#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

#ifndef DECOMPRESS_D_VEC
#define DECOMPRESS_D_VEC 1
#endif

namespace decrypt_g4 {

constexpr int32_t kUnpackN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kUnpackK = static_cast<int32_t>(F203_DECRYPT_K);
constexpr int32_t kUnpackQ = static_cast<int32_t>(F203_DECRYPT_Q);
constexpr int32_t kDecompBias11 = 1024;
constexpr int32_t kDecompBias5 = 16;
constexpr int32_t kDecompShift11 = 11;
constexpr int32_t kDecompShift5 = 5;

/** 标量 Decompress₁₁。 */
__aicore__ inline uint32_t decompress_d11_u32(uint32_t u)
{
    return ((u * static_cast<uint32_t>(kUnpackQ)) + 1024u) >> 11;
}

/** 标量 Decompress₅。 */
__aicore__ inline uint32_t decompress_d5_u32(uint32_t u)
{
    return ((u * static_cast<uint32_t>(kUnpackQ)) + 16u) >> 5;
}

/**
 * ByteDecode_d：从字节流按 LSB-first 抽出 N 个 d-bit 整数。
 * @param out  系数缓冲 [N]
 * @param in   编码字节（d=11→352B / d=5→160B）
 * @param dBits 11（c₁）或 5（c₂）
 */
__aicore__ inline void byte_decode_bits_scalar(int32_t *out, const uint8_t *in, uint32_t dBits)
{
    uint32_t bitPos = 0U;
    const uint32_t mask = (dBits >= 32U) ? 0xFFFFFFFFu : ((1U << dBits) - 1U);
    for (uint32_t i = 0; i < static_cast<uint32_t>(kUnpackN); ++i) {
        uint32_t a = 0U;
        /* 逐 bit 拼装第 i 个系数（FIPS ByteDecode 语义） */
        for (uint32_t j = 0; j < dBits; ++j) {
            const uint32_t byteIdx = bitPos >> 3;
            const uint32_t bitIdx = bitPos & 7U;
            if ((in[byteIdx] >> bitIdx) & 1U) {
                a |= (1U << j);
            }
            ++bitPos;
        }
        out[i] = static_cast<int32_t>(a & mask);
    }
}

/** 向量 Decompress：out = (in*q + bias) >> shift。 */
__aicore__ inline void decompress_d_vec(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                        AscendC::LocalTensor<int32_t> &tmp, int32_t bias, int32_t shift)
{
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    Muls(tmp, in, kUnpackQ, kUnpackN);
    Adds(tmp, tmp, bias, kUnpackN);
    ShiftRight(out, tmp, shift, kUnpackN);
}

/**
 * 在已分配的 UB 上：标量 Decode → 向量 Decompress → DataCopy 到 GM。
 * 同一 prep kernel 内复用同一组 LocalTensor（单 TPipe）。
 */
__aicore__ inline void unpack_one_poly_ub(const uint8_t *cPoly, __gm__ int32_t *gmOut, uint32_t dBits,
                                          AscendC::LocalTensor<int32_t> &inLocal,
                                          AscendC::LocalTensor<int32_t> &outLocal,
                                          AscendC::LocalTensor<int32_t> &tmp)
{
    int32_t compHost[kUnpackN];
    byte_decode_bits_scalar(compHost, cPoly, dBits);
    for (int32_t i = 0; i < kUnpackN; ++i) {
        inLocal.SetValue(i, compHost[i]);
    }
    AscendC::PipeBarrier<PIPE_ALL>();

#if DECOMPRESS_D_VEC >= 1
    const int32_t bias = (dBits == 11U) ? kDecompBias11 : kDecompBias5;
    const int32_t shift = (dBits == 11U) ? kDecompShift11 : kDecompShift5;
    decompress_d_vec(outLocal, inLocal, tmp, bias, shift);
#else
    (void)tmp;
    for (int32_t i = 0; i < kUnpackN; ++i) {
        const uint32_t u = static_cast<uint32_t>(inLocal.GetValue(i));
        if (dBits == 11U) {
            outLocal.SetValue(i, static_cast<int32_t>(decompress_d11_u32(u)));
        } else {
            outLocal.SetValue(i, static_cast<int32_t>(decompress_d5_u32(u)));
        }
    }
#endif
    AscendC::PipeBarrier<PIPE_ALL>();

    AscendC::GlobalTensor<int32_t> gm;
    gm.SetGlobalBuffer(gmOut, static_cast<uint32_t>(kUnpackN));
    AscendC::DataCopy(gm, outLocal, static_cast<uint32_t>(kUnpackN));
    AscendC::PipeBarrier<PIPE_ALL>();
}

/**
 * Alg.15 行 3–4：c → u'/v'。
 * @param cGm  密文 c₁‖c₂（1568B）
 * @param uGm  输出 u' polyvec [k×N] int32（Decompress₁₁）
 * @param vGm  输出 v' poly [N] int32（Decompress₅）
 * 前置：仅 AIV0 调用；中间态不回 Host。
 */
/**
 * Alg.15 行 3–4 入口：c → u'/v'（Decode 标量 + Decompress 向量，UB 落盘）。
 * @param cGm 密文；uGm/vGm 输出 int32
 * 前置：仅 AIV0。
 */
__aicore__ inline void unpack_c_impl(GM_ADDR cGm, GM_ADDR uGm, GM_ADDR vGm)
{
    const auto *cIn = reinterpret_cast<const __gm__ uint8_t *>(cGm);
    auto *uOut = reinterpret_cast<__gm__ int32_t *>(uGm);
    auto *vOut = reinterpret_cast<__gm__ int32_t *>(vGm);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufTmp;
    const uint32_t bytes = static_cast<uint32_t>(kUnpackN) * static_cast<uint32_t>(sizeof(int32_t));
    pipe.InitBuffer(queIn, 1, bytes);
    pipe.InitBuffer(queOut, 1, bytes);
    pipe.InitBuffer(bufTmp, bytes);

    AscendC::LocalTensor<int32_t> inLocal = queIn.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> outLocal = queOut.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> tmp = bufTmp.Get<int32_t>();

    /* c₁：k 个 poly，各 352B → Decompress₁₁ → u'[p] */
    for (int32_t p = 0; p < kUnpackK; ++p) {
        uint8_t cPolyLocal[F203_C1_POLY_BYTES];
        const uint32_t cOff = static_cast<uint32_t>(p) * F203_C1_POLY_BYTES;
        for (uint32_t b = 0; b < F203_C1_POLY_BYTES; ++b) {
            cPolyLocal[b] = cIn[cOff + b];
        }
        unpack_one_poly_ub(cPolyLocal, uOut + p * kUnpackN, 11U, inLocal, outLocal, tmp);
    }

    /* c₂：160B → Decompress₅ → v' */
    uint8_t c2Local[F203_C2_BYTES];
    for (uint32_t b = 0; b < F203_C2_BYTES; ++b) {
        c2Local[b] = cIn[F203_C1_BYTES + b];
    }
    unpack_one_poly_ub(c2Local, vOut, 5U, inLocal, outLocal, tmp);

    queIn.FreeTensor(inLocal);
    queOut.FreeTensor(outLocal);
}

} // namespace decrypt_g4

#endif
