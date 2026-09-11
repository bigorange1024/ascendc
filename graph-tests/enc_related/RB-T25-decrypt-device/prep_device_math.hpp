/**
 * @file prep_device_math.hpp
 * @brief RB-T25 Launch1：dk_pke+c → ŝ/u/v（AIV 标量解码 + Decompress）。
 *
 * 流水线：prep AIV-only；无 CrossCore。
 * 契约：
 *   - ŝ ← ByteDecode₁₂(dk)（shared 标量路径，直读 H2D 的 dkIn）
 *   - u ← Decompress₁₁(ByteDecode₁₁(c₁))；v ← Decompress₅(ByteDecode₅(c₂))
 *   - ByteDecode_d：先 DataCopy 字节入 UB，再 LSB-first 解（禁依赖标量 GM SetValue 镜像）
 *   - Decompress_d：(c·q + 2^{d-1}) >> d
 *
 * NPU 根因假设（相对 CPU 假绿）：
 *   旧路径用 GlobalTensor::SetValue 镜像 dk/c 到 ws，再标量读 ws —— 实机 GM 标量写
 *   不可靠；改为直读 dkIn/cIn（Host H2D）+ DataCopy 写出 ŝ/u/v。
 */
#ifndef RB_T25_PREP_DEVICE_MATH_HPP
#define RB_T25_PREP_DEVICE_MATH_HPP

#include "f203_byte_codec/byte_decode12_vec.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#include <cstdint>

namespace rb_t25 {

/** UB 字节流第 bitIdx 位（字节内 LSB-first）。 */
__aicore__ inline int32_t GetBitLsbUb(const AscendC::LocalTensor<uint8_t> &bytes, int32_t bitIdx)
{
    const uint32_t byteI = static_cast<uint32_t>(bitIdx / 8);
    const uint32_t bitI = static_cast<uint32_t>(bitIdx % 8);
    return static_cast<int32_t>((bytes.GetValue(byteI) >> bitI) & 1u);
}

/**
 * FIPS 203 Alg.6 ByteDecode_d：已在 UB 的 B^{32d} → Z_{2^d}^{256}。
 */
__aicore__ inline void ByteDecodeDFromUb(AscendC::LocalTensor<int32_t> &out,
                                         const AscendC::LocalTensor<uint8_t> &bytes, int32_t d)
{
    for (int32_t i = 0; i < tiling::kPolyN; ++i) {
        int32_t v = 0;
        for (int32_t j = 0; j < d; ++j) {
            v |= GetBitLsbUb(bytes, i * d + j) << j;
        }
        out.SetValue(static_cast<uint32_t>(i), v);
    }
}

/** Decompress_d：round(c·q/2^d) = (c·q + 2^{d-1}) >> d。 */
__aicore__ inline int32_t DecompressD(int32_t c, int32_t d)
{
    const int32_t bias = 1 << (d - 1);
    return (c * tiling::kQ + bias) >> d;
}

/**
 * prep：直读 dkIn/cIn → 解码写 ŝ/u/v（DataCopy 落 GM）。
 * 不依赖 ws 上 dk/c 的标量镜像（NPU 上 SetValue 不可靠）。
 */
__aicore__ inline void PrepUnpackDecrypt(GM_ADDR dkIn, GM_ADDR cIn, GM_ADDR ws)
{
    using namespace tiling;

    AscendC::GlobalTensor<int32_t> gmSHat;
    AscendC::GlobalTensor<int32_t> gmU;
    AscendC::GlobalTensor<int32_t> gmV;
    AscendC::GlobalTensor<uint8_t> gmCIn;
    gmSHat.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_S_HAT),
                           static_cast<uint32_t>(kKem * kPolyN));
    gmU.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_U),
                        static_cast<uint32_t>(kKem * kPolyN));
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(ws + OFF_V),
                        static_cast<uint32_t>(kPolyN));
    gmCIn.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(cIn),
                          static_cast<uint32_t>(kCBytes));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> quePoly;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queBytes;
    // c₁ poly=352B、c₂=160B；取较大缓冲
    constexpr uint32_t kMaxCPolyBytes = static_cast<uint32_t>(32 * kDu); // 352
    pipe.InitBuffer(quePoly, 1, static_cast<uint32_t>(kPolyBytes));
    pipe.InitBuffer(queBytes, 1, kMaxCPolyBytes);

    AscendC::LocalTensor<int32_t> poly = quePoly.AllocTensor<int32_t>();
    AscendC::LocalTensor<uint8_t> bytes = queBytes.AllocTensor<uint8_t>();

    // ŝ ← ByteDecode₁₂(dkIn)：直读 H2D（与 T05/T18 同构，禁 ws SetValue 镜像）
    for (int32_t p = 0; p < kKem; ++p) {
        const __gm__ uint8_t *row =
            reinterpret_cast<__gm__ uint8_t *>(dkIn) + static_cast<uint32_t>(p * 384);
        f203_byte_codec::poly_byte_decode12_scalar_gm(poly, row, kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmSHat[static_cast<uint32_t>(p * kPolyN)], poly,
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    // u ← Decompress₁₁(ByteDecode₁₁(c₁))：DataCopy 入 UB 再解
    constexpr int32_t kC1PolyBytes = 32 * kDu; // 352
    for (int32_t p = 0; p < kKem; ++p) {
        const uint32_t off = static_cast<uint32_t>(p * kC1PolyBytes);
        AscendC::DataCopy(bytes, gmCIn[off], static_cast<uint32_t>(kC1PolyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        ByteDecodeDFromUb(poly, bytes, kDu);
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

    // v ← Decompress₅(ByteDecode₅(c₂))
    {
        AscendC::DataCopy(bytes, gmCIn[static_cast<uint32_t>(kC1Bytes)],
                          static_cast<uint32_t>(kC2Bytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        ByteDecodeDFromUb(poly, bytes, kDv);
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
    queBytes.FreeTensor(bytes);
}

} // namespace rb_t25

#endif
