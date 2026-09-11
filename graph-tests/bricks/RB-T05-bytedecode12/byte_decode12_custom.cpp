/**
 * @file byte_decode12_custom.cpp
 * @brief RB-T05：ByteDecode₁₂ 独立终态 — ek 的 12-bit 载荷 → t̂[k·256]。
 *
 * 本文件在流水线中的位置：Encrypt 重建积木 G2；Host 读 input/ek_t_hat.bin
 * （k×384B），写出 output/t_hat.bin（k×256 int32）。算法委托
 * library/shared/f203_byte_codec/byte_decode12_vec.hpp 标量生产路径；本文件仅薄壳。
 *
 * 背景：inventory G2 — Decode₁₂ 无独立终态探针。
 * 结论：k=4、单 AIV、逐 poly 调 poly_byte_decode12_scalar_gm。
 * 未采用：从 alg14 搬文件；Gather 解交织（shared 头已废弃该路线）。
 */
#include "f203_byte_codec/byte_decode12_vec.hpp"
#include "kernel_operator.h"

namespace {
constexpr int32_t kK = 4;           // ML-KEM-1024
constexpr int32_t kPolyN = 256;
constexpr int32_t kPolyBytes = 384; // 12*256/8
constexpr int32_t kInBytes = kK * kPolyBytes;   // 1536
constexpr int32_t kOutCoeffs = kK * kPolyN;     // 1024
}  // namespace

/**
 * AIV-only：ByteEncode₁₂(t̂) 字节流 → t̂ 系数。
 * @param ekTHatGm 输入 GM，uint8[1536]（仅 t̂ 的 BE₁₂ 载荷，不含 ρ）
 * @param tHatGm   输出 GM，int32[1024]，按 poly 连续排布
 * 前置条件：blockDim=1；无 CrossCore。
 */
extern "C" __global__ __aicore__ void byte_decode12_custom(GM_ADDR ekTHatGm, GM_ADDR tHatGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    AscendC::GlobalTensor<int32_t> gmOut;
    gmOut.SetGlobalBuffer((__gm__ int32_t *)tHatGm, static_cast<uint32_t>(kOutCoeffs));
    (void)kInBytes;  // 契约：输入总长 k×384；标量路径按 poly 偏移直读 GM

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    pipe.InitBuffer(queOut, 1, static_cast<uint32_t>(kPolyN) * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> outLocal = queOut.AllocTensor<int32_t>();

    // 逐 poly：shared 标量路径直接从 GM 读字节；解完后搬出该 poly 的 256 系数。
    for (int32_t p = 0; p < kK; ++p) {
        const uint32_t byteOff = static_cast<uint32_t>(p * kPolyBytes);
        const __gm__ uint8_t *row = (__gm__ uint8_t *)(ekTHatGm) + byteOff;
        f203_byte_codec::poly_byte_decode12_scalar_gm(outLocal, row, kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmOut[static_cast<uint32_t>(p * kPolyN)], outLocal,
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    queOut.FreeTensor(outLocal);
}
