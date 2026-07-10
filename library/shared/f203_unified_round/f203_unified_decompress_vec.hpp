/**
 * @file f203_unified_decompress_vec.hpp
 * @brief FIPS 203 Decompress_d — 统一整数舍入设备实现（Muls(q)+Adds(bias)+ShiftRight(d)）。
 *
 * 本文件在流水线中的位置：被 pass-f203-decompress-unified-int-vec-k4 探针 kernel include；
 * 对齐 docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md §3。
 * 与 golden 的关系：向量/标量分支均与 decompress_unified_int_ref.c 同公式。
 *
 * DECOMPRESS_UNIFIED_INT_VEC=1（默认）：全 poly int32 向量三条指令，含 d=1。
 * DECOMPRESS_UNIFIED_INT_VEC=0：标量 GetValue/SetValue 对照。
 */
#ifndef F203_UNIFIED_DECOMPRESS_VEC_HPP
#define F203_UNIFIED_DECOMPRESS_VEC_HPP

#include "f203_unified_round_params.hpp"
#include "kernel_operator.h"

#ifndef F203_MLKEM_N
#error "include f203_mlkem_params.h before f203_unified_decompress_vec.hpp"
#endif

#ifndef F203_MLKEM_Q
#error "include f203_mlkem_params.h before f203_unified_decompress_vec.hpp"
#endif

namespace f203_unified_round {

constexpr uint32_t kPolyLen = static_cast<uint32_t>(F203_MLKEM_N);

#if DECOMPRESS_UNIFIED_INT_VEC >= 1

/**
 * 向量版 Decompress_d：out = (in * q + bias) >> d。
 * @param out 输出 int32 UB，长度 kPolyLen，解压后 canonical 系数。
 * @param in 输入 int32 UB，长度 kPolyLen，压缩域值 ∈ [0, 2^d-1]。
 * @param tmp scratch int32 UB，长度 kPolyLen，承载乘加中间结果。
 * 数值安全：d≤11 时 in*q < 2^11·3329 ≈ 6.8M，int32 无溢出。
 */
__aicore__ inline void poly_decompress_unified_local(AscendC::LocalTensor<int32_t> &out,
                                                     AscendC::LocalTensor<int32_t> &in,
                                                     AscendC::LocalTensor<int32_t> &tmp)
{
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    const int32_t n = static_cast<int32_t>(kPolyLen);
    Muls(tmp, in, static_cast<int32_t>(F203_MLKEM_Q), n);
    Adds(tmp, tmp, static_cast<int32_t>(F203_UNIFIED_DECOMPRESS_BIAS), n);
    ShiftRight(out, tmp, F203_UNIFIED_ROUND_D_BITS, n);
}

#else

__aicore__ inline void poly_decompress_unified_local(AscendC::LocalTensor<int32_t> &out,
                                                     AscendC::LocalTensor<int32_t> &in,
                                                     AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t i = 0; i < kPolyLen; ++i) {
        const uint32_t c = static_cast<uint32_t>(in.GetValue(static_cast<int32_t>(i)));
        out.SetValue(static_cast<int32_t>(i),
                     static_cast<int32_t>((c * static_cast<uint32_t>(F203_MLKEM_Q) +
                                           static_cast<uint32_t>(F203_UNIFIED_DECOMPRESS_BIAS)) >>
                                          F203_UNIFIED_ROUND_D_BITS));
    }
}

#endif

} // namespace f203_unified_round

#endif
