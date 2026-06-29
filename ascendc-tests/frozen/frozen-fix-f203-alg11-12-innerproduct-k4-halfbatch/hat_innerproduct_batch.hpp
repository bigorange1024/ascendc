#pragma once

#include "alg11_ub_load.hpp"
#include "alg11_vec_pipe.hpp"
#include "innerproduct_tiling.h"
#include "kernel_operator.h"
#include "multiply_ntts_vec.hpp"

namespace hat_ip {

constexpr int32_t kHalfLen = innerproduct_tiling::kHalfLen;
constexpr int32_t kHalfPairs = innerproduct_tiling::kHalfPairCount;

__aicore__ inline void deinterleave_half_pairs(AscendC::LocalTensor<int32_t> &even,
                                               AscendC::LocalTensor<int32_t> &odd,
                                               const AscendC::LocalTensor<int32_t> &aos, int32_t pairCount)
{
    for (int32_t i = 0; i < pairCount; ++i) {
        even.SetValue(static_cast<uint32_t>(i), aos.GetValue(static_cast<uint32_t>(i * 2)));
        odd.SetValue(static_cast<uint32_t>(i), aos.GetValue(static_cast<uint32_t>(i * 2 + 1)));
    }
}

/** f 侧 half：仅 Gather a0/a1（勿 four_lanes_gather(f,f) 污染 b 槽）。 */
__aicore__ inline void gather_f_half_pairs(alg11_vec::VecWs &w, const AscendC::LocalTensor<int32_t> &fHalf,
                                           int32_t pairCount)
{
    using AscendC::Gather;

    const uint32_t n = static_cast<uint32_t>(pairCount);
#if ALG11_MEM_OPS == 1 && !defined(ASCENDC_CPU_DEBUG)
    Gather(w.a0, fHalf, w.idx.ReinterpretCast<uint32_t>(), 0U, n);
    Gather(w.a1, fHalf, w.idx2.ReinterpretCast<uint32_t>(), 0U, n);
#else
    deinterleave_half_pairs(w.a0, w.a1, fHalf, pairCount);
#endif
}

__aicore__ inline void cache_s_half_blanes(AscendC::LocalTensor<int32_t> &b0, AscendC::LocalTensor<int32_t> &b1,
                                           const AscendC::LocalTensor<int32_t> &sHalf, alg11_vec::VecWs &w)
{
#if defined(ASCENDC_CPU_DEBUG)
    deinterleave_half_pairs(b0, b1, sHalf, kHalfPairs);
#else
    alg11_vec::deinterleave_four_lanes_gather(w, sHalf, sHalf, kHalfPairs);
    AscendC::DataCopy(b0, w.b0, static_cast<uint32_t>(kHalfPairs));
    AscendC::DataCopy(b1, w.b1, static_cast<uint32_t>(kHalfPairs));
    ALG11_PIPE_MTE2();
#endif
}

/**
 * 半多项式 ∘（128 交错系数）；对齐 hat_alg11::multiply_ntts_half_vec。
 * ws 须 bind_vec_ws(..., kRomPairCount)；pairCount 传 kHalfPairs。
 */
__aicore__ inline void multiply_ntts_half_ub(AscendC::LocalTensor<int32_t> &h, const AscendC::LocalTensor<int32_t> &f,
                                            const AscendC::LocalTensor<int32_t> &g, alg11_vec::VecWs &w,
                                            alg11_vec::RomUbLuts &rom, AscendC::LocalTensor<int32_t> &gammaSlice,
                                            int32_t gammaOff)
{
    if (gammaOff != 0) {
        alg11_ub_load::copy_rom_int32_ub(gammaSlice, gAlg11GammasGm + gammaOff, kHalfPairs);
        ALG11_PIPE_MTE2();
        w.gammaV = gammaSlice;
    } else {
        w.gammaV = rom.gammaV;
    }
    alg11_vec::multiply_ntts_vec_dispatch(h, f, g, w, rom, kHalfPairs);
    w.gammaV = rom.gammaV;
    ALG11_PIPE_ALL();
}

/**
 * 半多项式 ∘：f 侧 deinterleave；b0/b1 来自 ŝ 缓存。
 */
__aicore__ inline void basemul_half_cached_s(AscendC::LocalTensor<int32_t> &hHalf,
                                             const AscendC::LocalTensor<int32_t> &fHalf,
                                             const AscendC::LocalTensor<int32_t> &b0Cached,
                                             const AscendC::LocalTensor<int32_t> &b1Cached, alg11_vec::VecWs &w,
                                             alg11_vec::RomUbLuts &rom, AscendC::LocalTensor<int32_t> &gammaSlice,
                                             int32_t gammaOff)
{
    if (gammaOff != 0) {
        alg11_ub_load::copy_rom_int32_ub(gammaSlice, gAlg11GammasGm + gammaOff, kHalfPairs);
        ALG11_PIPE_MTE2();
        w.gammaV = gammaSlice;
    } else {
        w.gammaV = rom.gammaV;
    }

#if defined(ASCENDC_CPU_DEBUG)
    deinterleave_half_pairs(w.a0, w.a1, fHalf, kHalfPairs);
    AscendC::DataCopy(w.b0, b0Cached, static_cast<uint32_t>(kHalfPairs));
    AscendC::DataCopy(w.b1, b1Cached, static_cast<uint32_t>(kHalfPairs));
#else
    gather_f_half_pairs(w, fHalf, kHalfPairs);
    AscendC::DataCopy(w.b0, b0Cached, static_cast<uint32_t>(kHalfPairs));
    AscendC::DataCopy(w.b1, b1Cached, static_cast<uint32_t>(kHalfPairs));
    ALG11_PIPE_MTE2();
#endif

    alg11_vec::alg12_elementwise_vec(w, kHalfPairs);
    alg11_vec::interleave_pairs_dispatch(hHalf, w, rom, kHalfPairs);
    w.gammaV = rom.gammaV;
    ALG11_PIPE_ALL();
}

} // namespace hat_ip
