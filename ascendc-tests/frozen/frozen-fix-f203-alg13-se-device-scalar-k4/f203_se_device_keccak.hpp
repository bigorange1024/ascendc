/**
 * @file f203_se_device_keccak.hpp
 * @brief 设备标量 SHA3 / SHAKE256（Keccak-f1600，与 tiny_sha3 / shake_general 语义一致）。
 */
#pragma once

#include "keccak_f1600.h"

namespace F203SeDeviceKeccak {

constexpr uint32_t SHAKE256_RATE = 136U;

__aicore__ inline uint64_t LoadPartialLane(const uint8_t *p, uint32_t n)
{
    uint64_t v = 0;
    for (uint32_t i = 0; i < n; ++i) {
        v |= static_cast<uint64_t>(p[i]) << (8U * i);
    }
    return v;
}

__aicore__ inline void XorBytes(uint64_t a[25], const uint8_t *p, uint32_t n)
{
    uint32_t lane = 0;
    while (n >= 8U) {
        a[lane] ^= LoadPartialLane(p, 8U);
        p += 8U;
        n -= 8U;
        ++lane;
    }
    if (n > 0U) {
        a[lane] ^= LoadPartialLane(p, n);
    }
}

__aicore__ inline void StoreOutputBytes(uint8_t *dst, const uint64_t a[25], uint32_t offset, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t pos = offset + i;
        const uint32_t lane = pos / 8U;
        const uint32_t shift = (pos % 8U) * 8U;
        dst[i] = static_cast<uint8_t>((a[lane] >> shift) & 0xffU);
    }
}

/** FIPS SHA3：mdlen=32 → SHA3-256；mdlen=64 → SHA3-512 */
__aicore__ inline void Sha3OneShot(uint8_t *md, int mdlen, const uint8_t *in, uint32_t inlen)
{
    const uint32_t rate = static_cast<uint32_t>(200 - 2 * mdlen);
    uint64_t a[25];
    for (int i = 0; i < 25; ++i) {
        a[i] = 0;
    }

    uint32_t offset = 0;
    while (offset + rate <= inlen) {
        XorBytes(a, in + offset, rate);
        KeccakF1600Kernel::PermuteChain(a);
        offset += rate;
    }

    const uint32_t rem = inlen - offset;
    XorBytes(a, in + offset, rem);

    const uint32_t suffixLane = rem / 8U;
    const uint32_t suffixShift = (rem % 8U) * 8U;
    a[suffixLane] ^= static_cast<uint64_t>(0x06U) << suffixShift;

    const uint32_t padPos = rate - 1U;
    const uint32_t padLane = padPos / 8U;
    const uint32_t padShift = (padPos % 8U) * 8U;
    a[padLane] ^= static_cast<uint64_t>(0x80U) << padShift;

    KeccakF1600Kernel::PermuteChain(a);

    for (int i = 0; i < mdlen; ++i) {
        const uint32_t lane = static_cast<uint32_t>(i) / 8U;
        const uint32_t shift = (static_cast<uint32_t>(i) % 8U) * 8U;
        md[i] = static_cast<uint8_t>((a[lane] >> shift) & 0xffU);
    }
}

__aicore__ inline void Shake256OneShot(uint8_t *out, uint32_t outlen, const uint8_t *in, uint32_t inlen)
{
    uint64_t a[25];
    for (int i = 0; i < 25; ++i) {
        a[i] = 0;
    }

    uint32_t offset = 0;
    while (offset + SHAKE256_RATE <= inlen) {
        XorBytes(a, in + offset, SHAKE256_RATE);
        KeccakF1600Kernel::PermuteChain(a);
        offset += SHAKE256_RATE;
    }

    const uint32_t rem = inlen - offset;
    XorBytes(a, in + offset, rem);

    const uint32_t suffixLane = rem / 8U;
    const uint32_t suffixShift = (rem % 8U) * 8U;
    a[suffixLane] ^= static_cast<uint64_t>(0x1fU) << suffixShift;

    const uint32_t padPos = SHAKE256_RATE - 1U;
    const uint32_t padLane = padPos / 8U;
    const uint32_t padShift = (padPos % 8U) * 8U;
    a[padLane] ^= static_cast<uint64_t>(0x80U) << padShift;

    KeccakF1600Kernel::PermuteChain(a);

    uint32_t produced = 0;
    while (produced < outlen) {
        uint32_t chunk = outlen - produced;
        if (chunk > SHAKE256_RATE) {
            chunk = SHAKE256_RATE;
        }
        StoreOutputBytes(out + produced, a, 0, chunk);
        produced += chunk;
        if (produced < outlen) {
            KeccakF1600Kernel::PermuteChain(a);
        }
    }
}

}  // namespace F203SeDeviceKeccak
