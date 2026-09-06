#pragma once

#include <cstdint>

/** 与 ops-math Shake128GeneralTilingData 布局一致；rate 区分 SHAKE128(168) / SHAKE256(136)。 */
struct ShakeGeneralTilingData {
    uint32_t batch;
    uint32_t maxMsgLen;
    uint32_t outLen;
    uint32_t rate;
    uint32_t blockDim;
    uint32_t groupSize;
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
};

constexpr uint32_t SHAKE128_RATE_BYTES = 168U;
constexpr uint32_t SHAKE256_RATE_BYTES = 136U;
