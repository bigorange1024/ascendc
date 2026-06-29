#ifndef BYTE_ENCODE12_AIV_HPP
#define BYTE_ENCODE12_AIV_HPP

#include <cstdint>

#include "kernel_operator.h"
#include "tiling.h"

/** FIPS 203 Alg.5 ByteEncode₁₂ — 设备嵌 C（标量），每 poly 384 字节。 */
namespace byte_encode12 {

constexpr uint32_t kPolyBytes = 384U;
constexpr uint32_t kPolyVecBytes = static_cast<uint32_t>(tiling::kHatK) * kPolyBytes;

__aicore__ inline void poly_byte_encode12(__gm__ uint8_t *r, __gm__ const int32_t *a, uint32_t coeffN)
{
    const uint32_t pairs = coeffN / 2U;
    for (uint32_t i = 0; i < pairs; ++i) {
        const uint16_t t0 = static_cast<uint16_t>(a[2U * i] & 0xFFF);
        const uint16_t t1 = static_cast<uint16_t>(a[2U * i + 1U] & 0xFFF);
        r[3U * i + 0U] = static_cast<uint8_t>(t0 & 0xFFU);
        r[3U * i + 1U] = static_cast<uint8_t>((t0 >> 8) | ((t1 << 4) & 0xF0U));
        r[3U * i + 2U] = static_cast<uint8_t>(t1 >> 4);
    }
}

} // namespace byte_encode12

/** Alg.13 行 19–20：ByteEncode₁₂(t̂)→ek_polyvec；ByteEncode₁₂(ŝ)→sk_polyvec（k=4）。 */
class AivByteEncode1319 {
public:
    __aicore__ inline AivByteEncode1319(uint32_t coeffN) : coeffN(coeffN) {}

    __aicore__ inline void Init(GM_ADDR ek_polyvec, GM_ADDR sk_polyvec, GM_ADDR t_hat, GM_ADDR shat_ehat)
    {
        ekAddr = reinterpret_cast<uintptr_t>(ek_polyvec);
        skAddr = reinterpret_cast<uintptr_t>(sk_polyvec);
        tAddr = reinterpret_cast<uintptr_t>(t_hat);
        sAddr = reinterpret_cast<uintptr_t>(shat_ehat);
    }

    __aicore__ inline void Process()
    {
        __gm__ uint8_t *ek = reinterpret_cast<__gm__ uint8_t *>(ekAddr);
        __gm__ uint8_t *sk = reinterpret_cast<__gm__ uint8_t *>(skAddr);
        __gm__ int32_t *t = reinterpret_cast<__gm__ int32_t *>(tAddr);
        __gm__ int32_t *s = reinterpret_cast<__gm__ int32_t *>(sAddr);

        for (uint16_t p = 0; p < tiling::kHatK; ++p) {
            const uint32_t rowOff = static_cast<uint32_t>(p) * coeffN;
            const uint32_t byteOff = static_cast<uint32_t>(p) * byte_encode12::kPolyBytes;
            byte_encode12::poly_byte_encode12(ek + byteOff, t + rowOff, coeffN);
            byte_encode12::poly_byte_encode12(sk + byteOff, s + rowOff, coeffN);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
    }

private:
    const uint32_t coeffN;
    uintptr_t ekAddr, skAddr, tAddr, sAddr;
};

#endif
