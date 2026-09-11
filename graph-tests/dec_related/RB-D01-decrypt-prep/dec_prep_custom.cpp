/**
 * @file dec_prep_custom.cpp
 * @brief DRW-D01 / RB-D01：Decrypt L1 prep — ByteDecode₁₂(dk_pke)→ŝ；unpack c→u,v。
 *
 * 本文件在流水线中的位置：Decrypt 三核拓扑的 **L1 AIV-only**（S0A 锁定）；写出
 * `ŝ/u/v` 供后续 L2a NTT+dot。与 golden：I/O 对拍 host/Python FIPS 契约 oracle
 * （本刀非 liboqs 权威；全链权威留给后续刀）。
 *
 * 背景：X13 禁与 Encaps `prep_custom` 撞名 → 本核 basename 必须 `dec_prep_custom`；
 * X12 禁 `GlobalTensor::SetValue` 写业务 GM → 写出经 UB+DataCopy；X14 禁 prep∥NTT。
 * 结论：单 AIV、`BLOCK_DIM=1`、零 CrossCore；输入 H2D 直读。
 * 未采用：抄 T25/alg15/examples decrypt；`prep_custom.cpp` 文件名。
 */
#include "dec_prep_helpers.hpp"
#include "f203_byte_codec/byte_decode12_vec.hpp"
#include "kernel_operator.h"

namespace {
constexpr int32_t kK = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kPolyBytes12 = 384;
constexpr int32_t kPolyBytes11 = 352;
constexpr int32_t kPolyBytes5 = 160;
constexpr int32_t kDkBytes = kK * kPolyBytes12;          // 1536
constexpr int32_t kCBytesU = kK * kPolyBytes11;          // 1408
constexpr int32_t kCBytes = kCBytesU + kPolyBytes5;      // 1568
constexpr int32_t kSHatCoeffs = kK * kPolyN;             // 1024
constexpr int32_t kUCoeffs = kK * kPolyN;                // 1024
constexpr uint32_t kByteTileInts = (384U + 3U) / 4U;     // 对齐到 int32 槽：≥384B
}  // namespace

/**
 * AIV-only Decrypt prep。
 * @param dkPkeGm 输入 GM uint8[1536]：dk_pke = ByteEncode₁₂(ŝ)
 * @param cGm     输入 GM uint8[1568]：ByteEncode₁₁(Compress₁₁(u))‖ByteEncode₅(Compress₅(v))
 * @param sHatGm  输出 GM int32[1024]：ŝ（k 个 poly 连续）
 * @param uGm     输出 GM int32[1024]：u = Decompress₁₁(ByteDecode₁₁(c₁))
 * @param vGm     输出 GM int32[256]：v = Decompress₅(ByteDecode₅(c₂))
 * 前置条件：blockDim=1；KERNEL_TYPE_AIV_ONLY；无 CrossCore。
 */
extern "C" __global__ __aicore__ void dec_prep_custom(GM_ADDR dkPkeGm, GM_ADDR cGm, GM_ADDR sHatGm, GM_ADDR uGm,
                                                      GM_ADDR vGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    (void)kDkBytes;
    (void)kCBytes;

    AscendC::GlobalTensor<int32_t> gmSHat;
    AscendC::GlobalTensor<int32_t> gmU;
    AscendC::GlobalTensor<int32_t> gmV;
    AscendC::GlobalTensor<uint8_t> gmC;
    gmSHat.SetGlobalBuffer((__gm__ int32_t *)sHatGm, static_cast<uint32_t>(kSHatCoeffs));
    gmU.SetGlobalBuffer((__gm__ int32_t *)uGm, static_cast<uint32_t>(kUCoeffs));
    gmV.SetGlobalBuffer((__gm__ int32_t *)vGm, static_cast<uint32_t>(kPolyN));
    gmC.SetGlobalBuffer((__gm__ uint8_t *)cGm, static_cast<uint32_t>(kCBytes));

    AscendC::TPipe pipe;
    // 复用缓冲：byteTile[384B] + comp[256] + out[256] + tmp[256]
    AscendC::TBuf<AscendC::TPosition::VECCALC> workBuf;
    const uint32_t workInts = kByteTileInts + 3U * static_cast<uint32_t>(kPolyN);
    pipe.InitBuffer(workBuf, workInts * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> work = workBuf.Get<int32_t>();
    AscendC::LocalTensor<uint8_t> byteTile = work.ReinterpretCast<uint8_t>();
    AscendC::LocalTensor<int32_t> compLocal = work[kByteTileInts];
    AscendC::LocalTensor<int32_t> outLocal = compLocal[kPolyN];
    AscendC::LocalTensor<int32_t> tmpLocal = outLocal[kPolyN];

    // ---------- ŝ：ByteDecode₁₂(dk_pke)，shared 标量生产路径直读 GM ----------
    for (int32_t p = 0; p < kK; ++p) {
        const uint32_t byteOff = static_cast<uint32_t>(p * kPolyBytes12);
        const __gm__ uint8_t *row = (__gm__ uint8_t *)(dkPkeGm) + byteOff;
        f203_byte_codec::poly_byte_decode12_scalar_gm(outLocal, row, kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        // X12：经 UB DataCopy 写出，禁止 GlobalTensor::SetValue
        AscendC::DataCopy(gmSHat[static_cast<uint32_t>(p * kPolyN)], outLocal, static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    // ---------- u：逐 poly ByteDecode₁₁ + Decompress₁₁ ----------
    for (int32_t p = 0; p < kK; ++p) {
        const uint32_t byteOff = static_cast<uint32_t>(p * kPolyBytes11);
        AscendC::DataCopy(byteTile, gmC[byteOff], static_cast<uint32_t>(kPolyBytes11));
        AscendC::PipeBarrier<PIPE_ALL>();
        dec_prep::poly_byte_decode_d11_local(compLocal, byteTile);
        AscendC::PipeBarrier<PIPE_ALL>();
        dec_prep::poly_decompress_d_local(outLocal, compLocal, tmpLocal, 11, dec_prep::kBiasD11);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmU[static_cast<uint32_t>(p * kPolyN)], outLocal, static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    // ---------- v：ByteDecode₅ + Decompress₅（c 尾 160B） ----------
    {
        const uint32_t byteOff = static_cast<uint32_t>(kCBytesU);
        AscendC::DataCopy(byteTile, gmC[byteOff], static_cast<uint32_t>(kPolyBytes5));
        AscendC::PipeBarrier<PIPE_ALL>();
        dec_prep::poly_byte_decode_d5_local(compLocal, byteTile);
        AscendC::PipeBarrier<PIPE_ALL>();
        dec_prep::poly_decompress_d_local(outLocal, compLocal, tmpLocal, 5, dec_prep::kBiasD5);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmV, outLocal, static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }
}
