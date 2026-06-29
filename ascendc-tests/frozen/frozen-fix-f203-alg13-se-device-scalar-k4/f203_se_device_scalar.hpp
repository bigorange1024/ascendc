/**
 * @file f203_se_device_scalar.hpp
 * @brief Alg.13 行 8–15 设备标量：Keccak SHA3/SHAKE256 + CBD（与 fips203_se_sample.c 同式）。
 */
#pragma once

#include "f203_se_device_keccak.hpp"
#include "kernel_operator.h"

#include <cstdint>

namespace F203SeDevice {

constexpr uint32_t K = 4U;
constexpr uint32_t N = 256U;
constexpr uint32_t Q = 3329U;
constexpr uint32_t PRF_BYTES = 128U;
constexpr uint32_t SRC_ROWS = 8U;

__aicore__ inline int U32ToDec(uint32_t v, char *out)
{
    char tmp[10];
    int n = 0;
    if (v == 0U) {
        tmp[n++] = '0';
    } else {
        while (v > 0U) {
            tmp[n++] = static_cast<char>('0' + (v % 10U));
            v /= 10U;
        }
    }
    for (int i = 0; i < n; ++i) {
        out[i] = tmp[n - 1 - i];
    }
    return n;
}

__aicore__ inline void DerandFromSeedD(uint32_t seed_d, uint8_t d[32])
{
    char msg[48];
    int pos = 0;
    msg[pos++] = 'e';
    msg[pos++] = 'x';
    msg[pos++] = 'p';
    msg[pos++] = '-';
    msg[pos++] = 'm';
    msg[pos++] = 'l';
    msg[pos++] = 'k';
    msg[pos++] = 'e';
    msg[pos++] = 'm';
    msg[pos++] = '-';
    msg[pos++] = 'f';
    msg[pos++] = '2';
    msg[pos++] = '0';
    msg[pos++] = '3';
    msg[pos++] = '-';
    msg[pos++] = '2';
    msg[pos++] = 's';
    msg[pos++] = '1';
    msg[pos++] = 'e';
    msg[pos++] = '-';
    msg[pos++] = 'k';
    msg[pos++] = '4';
    msg[pos++] = ':';
    msg[pos++] = 'S';
    msg[pos++] = 'E';
    msg[pos++] = 'E';
    msg[pos++] = 'D';
    msg[pos++] = '_';
    msg[pos++] = 'D';
    msg[pos++] = '=';
    pos += U32ToDec(seed_d, msg + pos);
    F203SeDeviceKeccak::Sha3OneShot(d, 32, reinterpret_cast<const uint8_t *>(msg), static_cast<uint32_t>(pos));
}

__aicore__ inline void HashGSigma(const uint8_t d[32], uint8_t sigma[32])
{
    uint8_t in[33];
    uint8_t out[64];
    for (int i = 0; i < 32; ++i) {
        in[i] = d[i];
    }
    in[32] = static_cast<uint8_t>(K & 0xFFU);
    F203SeDeviceKeccak::Sha3OneShot(out, 64, in, sizeof(in));
    for (int i = 0; i < 32; ++i) {
        sigma[i] = out[32 + i];
    }
}

__aicore__ inline void PrfShake256(uint8_t *out, uint32_t outlen, const uint8_t sigma[32], uint8_t nonce)
{
    uint8_t in[33];
    for (int i = 0; i < 32; ++i) {
        in[i] = sigma[i];
    }
    in[32] = nonce;
    F203SeDeviceKeccak::Shake256OneShot(out, outlen, in, sizeof(in));
}

__aicore__ inline uint32_t Load32Le(const uint8_t *buf)
{
    return static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) | (static_cast<uint32_t>(buf[3]) << 24);
}

__aicore__ inline void SamplePolyCbd2Row(int32_t *dst_row, const uint8_t *buf)
{
    for (uint32_t i = 0; i < N / 8U; ++i) {
        const uint32_t t = Load32Le(buf + 4U * i);
        const uint32_t d = (t & 0x55555555U) + ((t >> 1) & 0x55555555U);
        for (uint32_t j = 0; j < 8U; ++j) {
            int32_t a = static_cast<int32_t>((d >> (4U * j + 0U)) & 0x3U);
            int32_t b = static_cast<int32_t>((d >> (4U * j + 2U)) & 0x3U);
            int32_t c = a - b;
            if (c < 0) {
                c += static_cast<int32_t>(Q);
            }
            c %= static_cast<int32_t>(Q);
            dst_row[8U * i + j] = c;
        }
    }
}

__aicore__ inline void BuildSrcFromSeedD(uint32_t seed_d, __gm__ int32_t *src_gm)
{
    uint8_t d[32];
    uint8_t sigma[32];
    uint8_t prf_buf[PRF_BYTES];
    int32_t row[N];

    DerandFromSeedD(seed_d, d);
    HashGSigma(d, sigma);

    for (uint8_t nonce = 0; nonce < SRC_ROWS; ++nonce) {
        PrfShake256(prf_buf, PRF_BYTES, sigma, nonce);
        SamplePolyCbd2Row(row, prf_buf);
        const uint32_t base = static_cast<uint32_t>(nonce) * N;
        for (uint32_t i = 0; i < N; ++i) {
            src_gm[base + i] = row[i];
        }
    }
}

}  // namespace F203SeDevice
