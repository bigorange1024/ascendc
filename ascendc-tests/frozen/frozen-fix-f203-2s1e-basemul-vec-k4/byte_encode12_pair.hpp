#ifndef BYTE_ENCODE12_PAIR_HPP
#define BYTE_ENCODE12_PAIR_HPP

#include "byte_encode12_config.hpp"
#include "hat_line18_2s1e.hpp"
#include "kernel_operator.h"
#include "tiling.h"
#if BYTE_ENCODE12_VEC >= 1
#include "byte_encode12_vec.hpp"
#endif

namespace byte_encode12 {

#if BYTE_ENCODE12_VEC < 1
constexpr uint32_t kPolyBytes = 384U;
constexpr uint32_t kPolysPerAiv = static_cast<uint32_t>(tiling::kEPerAiv);
constexpr uint32_t kAivShardBytes = kPolysPerAiv * kPolyBytes;
constexpr uint32_t kVecScratchBytes = 0U;
constexpr uint32_t kVecScratchInt32Slots = 0U;
#endif

__aicore__ inline void poly_byte_encode12_scalar(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &a, uint32_t coeffN)
{
    const uint32_t pairs = coeffN / 2U;
    for (uint32_t i = 0; i < pairs; ++i) {
        const uint16_t t0 = static_cast<uint16_t>(a.GetValue(2U * i) & 0xFFF);
        const uint16_t t1 = static_cast<uint16_t>(a.GetValue(2U * i + 1U) & 0xFFF);
        r.SetValue(3U * i + 0U, static_cast<uint8_t>(t0 & 0xFFU));
        r.SetValue(3U * i + 1U, static_cast<uint8_t>((t0 >> 8) | ((t1 << 4) & 0xF0U)));
        r.SetValue(3U * i + 2U, static_cast<uint8_t>(t1 >> 4));
    }
}

__aicore__ inline void poly_byte_encode12_local(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &a, uint32_t coeffN,
                                                LocalTensor<int32_t> &encodeWs)
{
#if BYTE_ENCODE12_VEC >= 1
    poly_byte_encode12_vec_local(r, a, coeffN, encodeWs);
#else
    (void)encodeWs;
    poly_byte_encode12_scalar(r, a, coeffN);
#endif
}

} // namespace byte_encode12

#endif
