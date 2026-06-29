#ifndef DECOMPRESS_D_VEC_HPP
#define DECOMPRESS_D_VEC_HPP

#include "decompress_d_config.hpp"
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
#if F203_DECOMPRESS_D == 4
    const int32_t bias = 8;
    const int32_t shift = 4;
#elif F203_DECOMPRESS_D == 10
    const int32_t bias = 512;
    const int32_t shift = 10;
#endif
    Muls(tmp, in, static_cast<int32_t>(F203_MLKEM_Q), n);
    Adds(tmp, tmp, bias, n);
    ShiftRight(out, tmp, shift, n);
}

#else

__aicore__ inline void poly_decompress_local(AscendC::LocalTensor<int32_t> &out, AscendC::LocalTensor<int32_t> &in,
                                             AscendC::LocalTensor<int32_t> &)
{
    for (uint32_t i = 0; i < kPolyLen; ++i) {
        const uint32_t u = static_cast<uint32_t>(in.GetValue(static_cast<int32_t>(i)));
#if F203_DECOMPRESS_D == 4
        out.SetValue(static_cast<int32_t>(i), static_cast<int32_t>((u * F203_MLKEM_Q + 8u) >> 4));
#else
        out.SetValue(static_cast<int32_t>(i), static_cast<int32_t>((u * F203_MLKEM_Q + 512u) >> 10));
#endif
    }
}

#endif

}  // namespace decompress_d

#endif
