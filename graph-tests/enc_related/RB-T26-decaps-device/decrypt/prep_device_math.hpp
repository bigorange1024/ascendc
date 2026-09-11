/**
 * @file prep_device_math.hpp
 * @brief RB-T25 Launch1：dk_pke+c → ŝ/u/v（AIV 标量解码 + Decompress）。
 *
 * 流水线：prep AIV-only；无 CrossCore。
 * 契约：
 *   - ŝ ← ByteDecode₁₂(dk)（shared 标量路径）
 *   - u ← Decompress₁₁(ByteDecode₁₁(c₁))；v ← Decompress₅(ByteDecode₅(c₂))
 *   - ByteDecode_d：FIPS Alg.6 按 bit 流 LSB-first（自研标量，非抄 decrypt 树）
 *   - Decompress_d：(c·q + 2^{d-1}) >> d（统一整数舍入笔记）
 */
#ifndef RB_T25_PREP_DEVICE_MATH_HPP
#define RB_T25_PREP_DEVICE_MATH_HPP

#include "f203_byte_codec/byte_decode12_vec.hpp"
#include "kernel_operator.h"
#include "decrypt/tiling.h"
#include <cstdint>

namespace rb_t25 {

/** 取字节流第 bitIdx 位（字节内 LSB-first）。 */
__aicore__ inline int32_t GetBitLsb(const __gm__ uint8_t *bytes, int32_t bitIdx)
{
    const int32_t byteI = bitIdx / 8;
    const int32_t bitI = bitIdx % 8;
    return static_cast<int32_t>((bytes[static_cast<uint32_t>(byteI)] >> bitI) & 1);
}

/**
 * FIPS 203 Alg.6 ByteDecode_d：B^{32d} → Z_{2^d}^{256}。
 * @param out UB int32[256]；bytes GM 起点；d∈{5,11}
 */
__aicore__ inline void ByteDecodeD(AscendC::LocalTensor<int32_t> &out, const __gm__ uint8_t *bytes,
                                   int32_t d)
{
    for (int32_t i = 0; i < dec_tiling::kPolyN; ++i) {
        int32_t v = 0;
        for (int32_t j = 0; j < d; ++j) {
            v |= GetBitLsb(bytes, i * d + j) << j;
        }
        out.SetValue(static_cast<uint32_t>(i), v);
    }
}

/**
 * Decompress_d：round(c·q/2^d) = (c·q + 2^{d-1}) >> d。
 */
__aicore__ inline int32_t DecompressD(int32_t c, int32_t d)
{
    const int32_t bias = 1 << (d - 1);
    return (c * dec_tiling::kQ + bias) >> d;
}

/**
 * prep：镜像 dk/c → 解码写 ŝ/u/v。
 * @param dkIn / cIn 独立输入 GM；ws 共享区
 */
__aicore__ inline void PrepUnpackDecrypt(GM_ADDR dkIn, GM_ADDR cIn, GM_ADDR ws)
{
    using namespace dec_tiling;

    AscendC::GlobalTensor<uint8_t> gmDkIn;
    AscendC::GlobalTensor<uint8_t> gmCIn;
    AscendC::GlobalTensor<uint8_t> gmDkWs;
    AscendC::GlobalTensor<uint8_t> gmCWs;
    AscendC::GlobalTensor<int32_t> gmSHat;
    AscendC::GlobalTensor<int32_t> gmU;
    AscendC::GlobalTensor<int32_t> gmV;
    gmDkIn.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(dkIn), static_cast<uint32_t>(kDkBytes));
    gmCIn.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(cIn), static_cast<uint32_t>(kCBytes));
    gmDkWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_DK),
                           static_cast<uint32_t>(kDkBytes));
    gmCWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ws + OFF_C),
                          static_cast<uint32_t>(kCBytes));
    gmSHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_S_HAT),
                           static_cast<uint32_t>(kKem * kPolyN));
    gmU.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_U),
                        static_cast<uint32_t>(kKem * kPolyN));
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_V),
                        static_cast<uint32_t>(kPolyN));

    // 镜像 dk / c 到 workspace（后续 launch 只读 ws）
    for (uint32_t i = 0; i < static_cast<uint32_t>(kDkBytes); ++i) {
        gmDkWs.SetValue(i, gmDkIn.GetValue(i));
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(kCBytes); ++i) {
        gmCWs.SetValue(i, gmCIn.GetValue(i));
    }
    AscendC::PipeBarrier<PIPE_ALL>();

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> quePoly;
    pipe.InitBuffer(quePoly, 1, static_cast<uint32_t>(kPolyBytes));
    AscendC::LocalTensor<int32_t> poly = quePoly.AllocTensor<int32_t>();

    // ŝ ← ByteDecode₁₂(dk)：逐 poly，384B/poly
    for (int32_t p = 0; p < kKem; ++p) {
        const __gm__ uint8_t *row =
            reinterpret_cast<__gm__ uint8_t *>(ws + OFF_DK) +
            static_cast<uint32_t>(p * 384);
        f203_byte_codec::poly_byte_decode12_scalar_gm(poly, row, kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmSHat[static_cast<uint32_t>(p * kPolyN)], poly,
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    // u ← Decompress₁₁(ByteDecode₁₁(c₁))；c₁ 按 poly 各 352B（32*11）
    constexpr int32_t kC1PolyBytes = 32 * kDu; // 352
    for (int32_t p = 0; p < kKem; ++p) {
        const __gm__ uint8_t *row =
            reinterpret_cast<__gm__ uint8_t *>(ws + OFF_C) +
            static_cast<uint32_t>(p * kC1PolyBytes);
        ByteDecodeD(poly, row, kDu);
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t i = 0; i < kPolyN; ++i) {
            const int32_t c = poly.GetValue(static_cast<uint32_t>(i));
            poly.SetValue(static_cast<uint32_t>(i), DecompressD(c, kDu));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmU[static_cast<uint32_t>(p * kPolyN)], poly,
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    // v ← Decompress₅(ByteDecode₅(c₂))；c₂ 在 c 尾 160B
    {
        const __gm__ uint8_t *row =
            reinterpret_cast<__gm__ uint8_t *>(ws + OFF_C) + static_cast<uint32_t>(kC1Bytes);
        ByteDecodeD(poly, row, kDv);
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t i = 0; i < kPolyN; ++i) {
            const int32_t c = poly.GetValue(static_cast<uint32_t>(i));
            poly.SetValue(static_cast<uint32_t>(i), DecompressD(c, kDv));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmV, poly, static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    quePoly.FreeTensor(poly);
}

} // namespace rb_t25

#endif
