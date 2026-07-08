/**
 * @file decompress_d_vec.hpp
 * @brief FIPS 203 Decompress_d 设备实现（d=4/5/10/11）。
 *
 * DECOMPRESS_D_VEC=1（默认）：Muls(q)+Adds(2^{d-1})+ShiftRight(d) 全 poly 向量；Decrypt 链推荐。
 * DECOMPRESS_D_VEC=0：标量 fallback。
 * 纯 per-lane，与 ByteDecode 标量 unpack 配对。见 docs/notes/F203-Compress-Decompress-向量实现指南.md、
 * docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md §4。
 */
#ifndef DECOMPRESS_D_VEC_HPP
#define DECOMPRESS_D_VEC_HPP

#include "decompress_d_config.hpp"
#include "f203_decompress_d_params.hpp"
#include "f203_mlkem_params.h"
#include "kernel_operator.h"

namespace decompress_d {

constexpr uint32_t kPolyLen = static_cast<uint32_t>(F203_MLKEM_N);

#if DECOMPRESS_D_VEC >= 1

__aicore__ inline void poly_decompress_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                             AscendC::LocalTensor<int32_t> &tmp)
{
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    const int32_t n = static_cast<int32_t>(kPolyLen);
    Muls(tmp, in, static_cast<int32_t>(F203_MLKEM_Q), n);
    Adds(tmp, tmp, static_cast<int32_t>(F203_DECOMPRESS_ROUND_BIAS), n);
    ShiftRight(out, tmp, F203_DECOMPRESS_D_BITS, n);
}

#else

__aicore__ inline void poly_decompress_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                             AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t i = 0; i < kPolyLen; ++i) {
        const uint32_t u = static_cast<uint32_t>(in.GetValue(static_cast<int32_t>(i)));
        out.SetValue(static_cast<int32_t>(i),
                     static_cast<int32_t>((u * static_cast<uint32_t>(F203_MLKEM_Q) +
                                           static_cast<uint32_t>(F203_DECOMPRESS_ROUND_BIAS)) >>
                                          F203_DECOMPRESS_D_BITS));
    }
}

#endif

} // namespace decompress_d

#endif
