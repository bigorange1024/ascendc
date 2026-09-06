#pragma once

#include <cstdint>

#include "shake_general_tiling_data.h"

inline uint32_t ShakeXofCeilDiv(uint32_t a, uint32_t b)
{
    return (a + b - 1U) / b;
}

inline uint32_t ShakeXofGcdU32(uint32_t a, uint32_t b)
{
    while (b != 0U) {
        const uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/** 与 ops-math shake128_general_tiling.cpp 一致；rate 由调用方指定（128→168，256→136）。 */
inline void FillShakeTiling(ShakeGeneralTilingData *tiling, uint32_t batch, uint32_t maxMsgLen, uint32_t outLen,
                            uint32_t rate)
{
    const uint32_t cacheLineBytes = 64U;
    const uint32_t groupSize = cacheLineBytes / ShakeXofGcdU32(outLen, cacheLineBytes);
    const uint32_t groupCount = ShakeXofCeilDiv(batch, groupSize);
    const uint32_t coreNum = 20U;
    const uint32_t blockDim = groupCount < coreNum ? groupCount : coreNum;

    tiling->batch = batch;
    tiling->maxMsgLen = maxMsgLen;
    tiling->outLen = outLen;
    tiling->rate = rate;
    tiling->blockDim = blockDim;
    tiling->groupSize = groupSize;
    tiling->reserved0 = 0U;
    tiling->reserved1 = 0U;
    tiling->reserved2 = 0U;
}
