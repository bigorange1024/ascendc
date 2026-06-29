#ifndef BYTE_ENCODE12_PAIR_HPP
#define BYTE_ENCODE12_PAIR_HPP

#include "hat_line18_2s1e.hpp"
#include "kernel_operator.h"
#include "tiling.h"

namespace byte_encode12 {

constexpr uint32_t kPolyBytes = 384U;
constexpr uint32_t kPolysPerAiv = static_cast<uint32_t>(tiling::kEPerAiv);
constexpr uint32_t kAivShardBytes = kPolysPerAiv * kPolyBytes;

__aicore__ inline void poly_byte_encode12_local(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &a, uint32_t coeffN)
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

} // namespace byte_encode12

#endif
