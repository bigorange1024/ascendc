/**
 * @file keccak_f1600.h
 * @brief AscendC Keccak-f[1600] permutation (device header-only primitives).
 *
 * Vendored from third-party ops-math fork. See SOURCE.md in this directory for
 * upstream URL, commit, and CANN Open Software License 2.0.
 */
#pragma once

#include "kernel_operator.h"
#include "keccak_f1600_tiling_data.h"

namespace KeccakF1600Kernel {

constexpr uint64_t RC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808aULL, 0x8000000080008000ULL,
    0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008aULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL,
    0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL,
    0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL,
};

constexpr int RHO[25] = {
     0,  1, 62, 28, 27,
    36, 44,  6, 55, 20,
     3, 10, 43, 25, 39,
    41, 45, 15, 21,  8,
    18,  2, 61, 56, 14,
};

__aicore__ inline uint64_t Rotl64(uint64_t x, int n)
{
    return n == 0 ? x : ((x << n) | (x >> (64 - n)));
}

__aicore__ inline uint64_t Rotl1(uint64_t x)
{
    return (x << 1) | (x >> 63);
}

__aicore__ inline uint64_t Load64Le(const __gm__ uint8_t* p)
{
    const __gm__ uint64_t* q = reinterpret_cast<const __gm__ uint64_t*>(p);
    return q[0];
}

__aicore__ inline void Store64Le(__gm__ uint8_t* p, uint64_t v)
{
    __gm__ uint64_t* q = reinterpret_cast<__gm__ uint64_t*>(p);
    q[0] = v;
}


__aicore__ inline void Permute(uint64_t a[25])
{
    for (int round = 0; round < 24; ++round) {
        uint64_t c[5];
        uint64_t d[5];
        uint64_t b[25];

        for (int x = 0; x < 5; ++x) {
            c[x] = a[x] ^ a[x + 5] ^ a[x + 10] ^ a[x + 15] ^ a[x + 20];
        }
        for (int x = 0; x < 5; ++x) {
            d[x] = c[(x + 4) % 5] ^ Rotl1(c[(x + 1) % 5]);
        }
        for (int y = 0; y < 5; ++y) {
            for (int x = 0; x < 5; ++x) {
                a[x + 5 * y] ^= d[x];
            }
        }

        for (int y = 0; y < 5; ++y) {
            for (int x = 0; x < 5; ++x) {
                int src = x + 5 * y;
                int dst = y + 5 * ((2 * x + 3 * y) % 5);
                b[dst] = Rotl64(a[src], RHO[src]);
            }
        }

        for (int y = 0; y < 5; ++y) {
            for (int x = 0; x < 5; ++x) {
                a[x + 5 * y] =
                    b[x + 5 * y] ^
                    ((~b[((x + 1) % 5) + 5 * y]) & b[((x + 2) % 5) + 5 * y]);
            }
        }

        a[0] ^= RC[round];
    }
}


__aicore__ inline void PermuteFast(uint64_t a[25])
{
    for (int round = 0; round < 24; ++round) {
        const uint64_t c0 = a[0] ^ a[5] ^ a[10] ^ a[15] ^ a[20];
        const uint64_t c1 = a[1] ^ a[6] ^ a[11] ^ a[16] ^ a[21];
        const uint64_t c2 = a[2] ^ a[7] ^ a[12] ^ a[17] ^ a[22];
        const uint64_t c3 = a[3] ^ a[8] ^ a[13] ^ a[18] ^ a[23];
        const uint64_t c4 = a[4] ^ a[9] ^ a[14] ^ a[19] ^ a[24];

        const uint64_t d0 = c4 ^ Rotl1(c1);
        const uint64_t d1 = c0 ^ Rotl1(c2);
        const uint64_t d2 = c1 ^ Rotl1(c3);
        const uint64_t d3 = c2 ^ Rotl1(c4);
        const uint64_t d4 = c3 ^ Rotl1(c0);

        a[0] ^= d0;
        a[1] ^= d1;
        a[2] ^= d2;
        a[3] ^= d3;
        a[4] ^= d4;
        a[5] ^= d0;
        a[6] ^= d1;
        a[7] ^= d2;
        a[8] ^= d3;
        a[9] ^= d4;
        a[10] ^= d0;
        a[11] ^= d1;
        a[12] ^= d2;
        a[13] ^= d3;
        a[14] ^= d4;
        a[15] ^= d0;
        a[16] ^= d1;
        a[17] ^= d2;
        a[18] ^= d3;
        a[19] ^= d4;
        a[20] ^= d0;
        a[21] ^= d1;
        a[22] ^= d2;
        a[23] ^= d3;
        a[24] ^= d4;

        const uint64_t b0 = a[0];
        const uint64_t b10 = Rotl64(a[1], 1);
        const uint64_t b20 = Rotl64(a[2], 62);
        const uint64_t b5 = Rotl64(a[3], 28);
        const uint64_t b15 = Rotl64(a[4], 27);
        const uint64_t b16 = Rotl64(a[5], 36);
        const uint64_t b1 = Rotl64(a[6], 44);
        const uint64_t b11 = Rotl64(a[7], 6);
        const uint64_t b21 = Rotl64(a[8], 55);
        const uint64_t b6 = Rotl64(a[9], 20);
        const uint64_t b7 = Rotl64(a[10], 3);
        const uint64_t b17 = Rotl64(a[11], 10);
        const uint64_t b2 = Rotl64(a[12], 43);
        const uint64_t b12 = Rotl64(a[13], 25);
        const uint64_t b22 = Rotl64(a[14], 39);
        const uint64_t b23 = Rotl64(a[15], 41);
        const uint64_t b8 = Rotl64(a[16], 45);
        const uint64_t b18 = Rotl64(a[17], 15);
        const uint64_t b3 = Rotl64(a[18], 21);
        const uint64_t b13 = Rotl64(a[19], 8);
        const uint64_t b14 = Rotl64(a[20], 18);
        const uint64_t b24 = Rotl64(a[21], 2);
        const uint64_t b9 = Rotl64(a[22], 61);
        const uint64_t b19 = Rotl64(a[23], 56);
        const uint64_t b4 = Rotl64(a[24], 14);

        a[0] = b0 ^ b2 ^ (b1 & b2);
        a[1] = b1 ^ b3 ^ (b2 & b3);
        a[2] = b2 ^ b4 ^ (b3 & b4);
        a[3] = b3 ^ b0 ^ (b4 & b0);
        a[4] = b4 ^ b1 ^ (b0 & b1);
        a[5] = b5 ^ b7 ^ (b6 & b7);
        a[6] = b6 ^ b8 ^ (b7 & b8);
        a[7] = b7 ^ b9 ^ (b8 & b9);
        a[8] = b8 ^ b5 ^ (b9 & b5);
        a[9] = b9 ^ b6 ^ (b5 & b6);
        a[10] = b10 ^ b12 ^ (b11 & b12);
        a[11] = b11 ^ b13 ^ (b12 & b13);
        a[12] = b12 ^ b14 ^ (b13 & b14);
        a[13] = b13 ^ b10 ^ (b14 & b10);
        a[14] = b14 ^ b11 ^ (b10 & b11);
        a[15] = b15 ^ b17 ^ (b16 & b17);
        a[16] = b16 ^ b18 ^ (b17 & b18);
        a[17] = b17 ^ b19 ^ (b18 & b19);
        a[18] = b18 ^ b15 ^ (b19 & b15);
        a[19] = b19 ^ b16 ^ (b15 & b16);
        a[20] = b20 ^ b22 ^ (b21 & b22);
        a[21] = b21 ^ b23 ^ (b22 & b23);
        a[22] = b22 ^ b24 ^ (b23 & b24);
        a[23] = b23 ^ b20 ^ (b24 & b20);
        a[24] = b24 ^ b21 ^ (b20 & b21);

        a[0] ^= RC[round];
    }
}



__aicore__ inline void PermuteChain(uint64_t a[25])
{
    for (int round = 0; round < 24; ++round) {
        const uint64_t c0 = a[0] ^ a[5] ^ a[10] ^ a[15] ^ a[20];
        const uint64_t c1 = a[1] ^ a[6] ^ a[11] ^ a[16] ^ a[21];
        const uint64_t c2 = a[2] ^ a[7] ^ a[12] ^ a[17] ^ a[22];
        const uint64_t c3 = a[3] ^ a[8] ^ a[13] ^ a[18] ^ a[23];
        const uint64_t c4 = a[4] ^ a[9] ^ a[14] ^ a[19] ^ a[24];

        const uint64_t d0 = c4 ^ Rotl1(c1);
        const uint64_t d1 = c0 ^ Rotl1(c2);
        const uint64_t d2 = c1 ^ Rotl1(c3);
        const uint64_t d3 = c2 ^ Rotl1(c4);
        const uint64_t d4 = c3 ^ Rotl1(c0);

        a[0] ^= d0;  a[5] ^= d0;  a[10] ^= d0; a[15] ^= d0; a[20] ^= d0;
        a[1] ^= d1;  a[6] ^= d1;  a[11] ^= d1; a[16] ^= d1; a[21] ^= d1;
        a[2] ^= d2;  a[7] ^= d2;  a[12] ^= d2; a[17] ^= d2; a[22] ^= d2;
        a[3] ^= d3;  a[8] ^= d3;  a[13] ^= d3; a[18] ^= d3; a[23] ^= d3;
        a[4] ^= d4;  a[9] ^= d4;  a[14] ^= d4; a[19] ^= d4; a[24] ^= d4;

        uint64_t t = a[1];
        uint64_t u = a[10]; a[10] = Rotl64(t, 1);  t = u;
        u = a[7];  a[7]  = Rotl64(t, 3);  t = u;
        u = a[11]; a[11] = Rotl64(t, 6);  t = u;
        u = a[17]; a[17] = Rotl64(t, 10); t = u;
        u = a[18]; a[18] = Rotl64(t, 15); t = u;
        u = a[3];  a[3]  = Rotl64(t, 21); t = u;
        u = a[5];  a[5]  = Rotl64(t, 28); t = u;
        u = a[16]; a[16] = Rotl64(t, 36); t = u;
        u = a[8];  a[8]  = Rotl64(t, 45); t = u;
        u = a[21]; a[21] = Rotl64(t, 55); t = u;
        u = a[24]; a[24] = Rotl64(t, 2);  t = u;
        u = a[4];  a[4]  = Rotl64(t, 14); t = u;
        u = a[15]; a[15] = Rotl64(t, 27); t = u;
        u = a[23]; a[23] = Rotl64(t, 41); t = u;
        u = a[19]; a[19] = Rotl64(t, 56); t = u;
        u = a[13]; a[13] = Rotl64(t, 8);  t = u;
        u = a[12]; a[12] = Rotl64(t, 25); t = u;
        u = a[2];  a[2]  = Rotl64(t, 43); t = u;
        u = a[20]; a[20] = Rotl64(t, 62); t = u;
        u = a[14]; a[14] = Rotl64(t, 18); t = u;
        u = a[22]; a[22] = Rotl64(t, 39); t = u;
        u = a[9];  a[9]  = Rotl64(t, 61); t = u;
        u = a[6];  a[6]  = Rotl64(t, 20); t = u;
        a[1] = Rotl64(t, 44);

        for (int y = 0; y < 25; y += 5) {
            const uint64_t b0 = a[y + 0];
            const uint64_t b1 = a[y + 1];
            const uint64_t b2 = a[y + 2];
            const uint64_t b3 = a[y + 3];
            const uint64_t b4 = a[y + 4];

            a[y + 0] = b0 ^ b2 ^ (b1 & b2);
            a[y + 1] = b1 ^ b3 ^ (b2 & b3);
            a[y + 2] = b2 ^ b4 ^ (b3 & b4);
            a[y + 3] = b3 ^ b0 ^ (b4 & b0);
            a[y + 4] = b4 ^ b1 ^ (b0 & b1);
        }

        a[0] ^= RC[round];
    }
}


class KernelKeccakF1600 {
public:
    __aicore__ inline KernelKeccakF1600() {}

    __aicore__ inline void Init(GM_ADDR x, GM_ADDR y, const KeccakF1600TilingData* tiling)
    {
        x_ = reinterpret_cast<__gm__ uint8_t*>(x);
        y_ = reinterpret_cast<__gm__ uint8_t*>(y);
        batch_ = tiling->batch;
        blockDim_ = tiling->blockDim;
        statesPerCore_ = tiling->statesPerCore;
    }

    __aicore__ inline void Process()
    {
        const uint32_t blockIdx = AscendC::GetBlockIdx();
        const uint32_t realBlockNum = AscendC::GetBlockNum();
        const uint32_t groupSize = 16;
        const uint32_t groupCount = (batch_ + groupSize - 1U) / groupSize;

        for (uint32_t groupIdx = blockIdx; groupIdx < groupCount; groupIdx += realBlockNum) {
            const uint32_t begin = groupIdx * groupSize;
            uint32_t end = begin + groupSize;
            if (end > batch_) {
                end = batch_;
            }

            for (uint32_t stateIdx = begin; stateIdx < end; ++stateIdx) {
                uint64_t a[25];
                const __gm__ uint8_t* src = x_ + stateIdx * 200U;
                __gm__ uint8_t* dst = y_ + stateIdx * 200U;
                for (int i = 0; i < 25; ++i) {
                    a[i] = Load64Le(src + i * 8);
                }
                PermuteFast(a);
                for (int i = 0; i < 25; ++i) {
                    Store64Le(dst + i * 8, a[i]);
                }
            }
        }
    }

private:
    __gm__ uint8_t* x_ = nullptr;
    __gm__ uint8_t* y_ = nullptr;
    uint32_t batch_ = 0;
    uint32_t blockDim_ = 1;
    uint32_t statesPerCore_ = 1;
};

}  // namespace KeccakF1600Kernel
