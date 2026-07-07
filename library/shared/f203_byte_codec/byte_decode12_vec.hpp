#pragma once

/**
 * @file byte_decode12_vec.hpp
 * @brief FIPS 203 ByteDecode₁₂（d=12）：标量生产 + **零 Gather** 向量备用。
 *
 * ## 向量备用（Alg7 两段式，无 Gather）
 *
 * 对齐 `pass-fix-f203-alg7-sample-ntt-k4/f203_alg7_d12_vec.hpp` 中 `ComputeD12Vec`：
 *   - 解交织：**标量** GetValue（Alg7 证实 Gather 解交织为负优化）
 *   - 算术：**向量** 128 lane（`compute_d12_pairs_vec`）
 *
 * ## 标量生产（`poly_byte_decode12_scalar_gm`）
 *
 * encrypt-compute 探针 fused 内 tick 最优（~113k）；`F203_BYTE_DECODE12_IMPL=0` 默认。
 *
 * ## 已废弃路线（勿复用）
 *
 * - Gather 解交织（tick ~145k）
 * - 零 Gather 但 4×32 tile 切分（tick ~139k，Barrier 过多）
 */
#include "kernel_operator.h"

namespace f203_byte_codec {

constexpr uint32_t kDecodePairs = 128U;
constexpr uint32_t kDecodePolyBytes = 384U;
/** byteTile[384B] + c0/c1/c2/t0/t1 各 128 int32 */
constexpr uint32_t kDecode12WsInts =
    (kDecodePolyBytes + sizeof(int32_t) - 1U) / sizeof(int32_t) + 5U * kDecodePairs;

struct Decode12Ws {
    AscendC::LocalTensor<uint8_t> byteTile;
    AscendC::LocalTensor<int32_t> c0;
    AscendC::LocalTensor<int32_t> c1;
    AscendC::LocalTensor<int32_t> c2;
    AscendC::LocalTensor<int32_t> t0;
    AscendC::LocalTensor<int32_t> t1;
};

__aicore__ inline void bind_decode12_ws(AscendC::LocalTensor<int32_t> &ws, Decode12Ws &w)
{
    auto base = ws.ReinterpretCast<uint8_t>();
    w.byteTile = base[0];
    w.c0 = base[kDecodePolyBytes].ReinterpretCast<int32_t>();
    w.c1 = w.c0[kDecodePairs];
    w.c2 = w.c1[kDecodePairs];
    w.t0 = w.c2[kDecodePairs];
    w.t1 = w.t0[kDecodePairs];
}

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

/** 标量解交织：byteTile[3*i+0..2] → c0/c1/c2[i]（与 Alg7 DeinterleaveCandScalarFromUb 同构）。 */
__aicore__ inline void deinterleave_c012_from_ub(AscendC::LocalTensor<uint8_t> &byteTile, Decode12Ws &w)
{
    for (uint32_t i = 0; i < kDecodePairs; ++i) {
        const uint32_t base = 3U * i;
        w.c0.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(byteTile.GetValue(base + 0U)));
        w.c1.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(byteTile.GetValue(base + 1U)));
        w.c2.SetValue(static_cast<int32_t>(i), static_cast<int32_t>(byteTile.GetValue(base + 2U)));
    }
}

/**
 * 向量 compute（128 lane 一次）：t0[i]=b0+256*(b1&15)，t1[i]=(b1>>4)+16*b2。
 * 与 Alg7 ComputeD12Vec 公式一致。
 */
__aicore__ inline void compute_d12_pairs_vec(Decode12Ws &w)
{
    using AscendC::Add;
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;

    const int32_t n = static_cast<int32_t>(kDecodePairs);

    Adds(w.t0, w.c1, 0, n);
    mask_low_bits_i32(w.t0, w.t1, 4, kDecodePairs);
    Muls(w.t1, w.t0, 256, n);
    Add(w.t0, w.c0, w.t1, n);

    ShiftRight(w.t1, w.c1, 4, n);
    Muls(w.c2, w.c2, 16, n);
    Add(w.t1, w.t1, w.c2, n);
}

/** 交织写回 out[2i]=t0[i], out[2i+1]=t1[i]。 */
__aicore__ inline void scatter_t01_interleave(AscendC::LocalTensor<int32_t> &out, Decode12Ws &w)
{
    for (uint32_t i = 0; i < kDecodePairs; ++i) {
        const int32_t ii = static_cast<int32_t>(i);
        out.SetValue(2U * i + 0U, w.t0.GetValue(ii));
        out.SetValue(2U * i + 1U, w.t1.GetValue(ii));
    }
}

/**
 * 零 Gather 向量备用：单 poly GM byte → UB int32[N]。
 * 流程：DataCopy → 标量解交织 c0/c1/c2 → 向量 compute_d12_pairs_vec → 标量交织。
 * 须 `F203_BYTE_DECODE12_IMPL=1`；tick 本探针 ~138k（高于标量 ~113k）。
 */
__aicore__ inline void poly_byte_decode12_alg7_gm(AscendC::LocalTensor<int32_t> &out, AscendC::GlobalTensor<uint8_t> &ekGm,
                                                  uint32_t byteOff, AscendC::LocalTensor<int32_t> &ws)
{
    Decode12Ws w;
    bind_decode12_ws(ws, w);

    AscendC::DataCopy(w.byteTile, ekGm[byteOff], kDecodePolyBytes);
    AscendC::PipeBarrier<PIPE_ALL>();

    deinterleave_c012_from_ub(w.byteTile, w);
    AscendC::PipeBarrier<PIPE_ALL>();

    compute_d12_pairs_vec(w);
    AscendC::PipeBarrier<PIPE_ALL>();

    scatter_t01_interleave(out, w);
}

/**
 * 标量 ByteDecode₁₂（生产默认）：单 poly，GM byte → UB int32[N]。
 * 128 pair 紧凑循环；encrypt-compute fused 内 tick 最优。
 */
__aicore__ inline void poly_byte_decode12_scalar_gm(AscendC::LocalTensor<int32_t> &out, const __gm__ uint8_t *ekRow,
                                                    int32_t coeffN)
{
    for (int32_t i = 0; i < coeffN / 2; ++i) {
        const uint32_t b0 = static_cast<uint32_t>(ekRow[3U * static_cast<uint32_t>(i) + 0U]);
        const uint32_t b1 = static_cast<uint32_t>(ekRow[3U * static_cast<uint32_t>(i) + 1U]);
        const uint32_t b2 = static_cast<uint32_t>(ekRow[3U * static_cast<uint32_t>(i) + 2U]);
        const uint32_t v0 = b0 | ((b1 & 0x0FU) << 8);
        const uint32_t v1 = (b1 >> 4) | (b2 << 4);
        out.SetValue(static_cast<uint32_t>(2U * static_cast<uint32_t>(i) + 0U), static_cast<int32_t>(v0));
        out.SetValue(static_cast<uint32_t>(2U * static_cast<uint32_t>(i) + 1U), static_cast<int32_t>(v1));
    }
}

} // namespace f203_byte_codec
