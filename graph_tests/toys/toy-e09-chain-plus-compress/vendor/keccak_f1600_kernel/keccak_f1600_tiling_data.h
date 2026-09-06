/**
 * @file keccak_f1600_tiling_data.h
 * @brief Tiling layout for standalone KeccakF1600 operator (vendored; see SOURCE.md).
 */
#pragma once

#include <stdint.h>

struct KeccakF1600TilingData {
    uint32_t batch;
    uint32_t blockDim;
    uint32_t statesPerCore;
    uint32_t reserved;
};
