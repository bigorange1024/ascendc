/**
 * Host 侧：从 ek_pke 内存解码 ρ / t̂（不落中间 .bin）。
 * 仅服务开场一次性 H2D；禁止把解码结果写成工程可读输入文件。
 */
#pragma once

#include <cstdint>
#include <cstring>

namespace host_ek {

constexpr size_t kEkTBytes = 1536;
constexpr size_t kEkBytes = 1568;
constexpr size_t kRhoBytes = 32;
constexpr int32_t kK = 4;
constexpr int32_t kN = 256;

/** Alg.6 ByteDecode₁₂：384B → int32[256]。 */
inline void ByteDecode12(int32_t *out256, const uint8_t *buf384)
{
    for (int i = 0; i < kN / 2; ++i) {
        const uint8_t b0 = buf384[3 * i + 0];
        const uint8_t b1 = buf384[3 * i + 1];
        const uint8_t b2 = buf384[3 * i + 2];
        out256[2 * i] = static_cast<int32_t>(b0) | (static_cast<int32_t>(b1 & 0x0F) << 8);
        out256[2 * i + 1] = static_cast<int32_t>(b1 >> 4) | (static_cast<int32_t>(b2) << 4);
    }
}

/** ek_pke = ByteEncode₁₂(t̂)‖ρ → 填 rho[32]、tHat[K·N]（Host 缓冲）。 */
inline bool DecodeEkPke(uint8_t *rho32, int32_t *tHatKn, const uint8_t *ek, size_t ekLen)
{
    if (ek == nullptr || rho32 == nullptr || tHatKn == nullptr || ekLen != kEkBytes) {
        return false;
    }
    std::memcpy(rho32, ek + kEkTBytes, kRhoBytes);
    for (int i = 0; i < kK; ++i) {
        ByteDecode12(tHatKn + i * kN, ek + static_cast<size_t>(i) * 384U);
    }
    return true;
}

} // namespace host_ek
