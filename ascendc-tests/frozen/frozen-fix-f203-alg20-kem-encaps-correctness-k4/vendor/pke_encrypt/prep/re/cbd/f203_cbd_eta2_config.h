/**
 * @file f203_cbd_eta2_config.h
 * @brief Host 侧布局常量（不含 kernel_operator.h）。
 */
#pragma once

#include <cstdint>

namespace F203CbdEta2Host {

constexpr uint32_t K = 4U;
constexpr uint32_t N = 256U;
constexpr uint32_t ETA = 2U;
constexpr uint32_t PRF_BYTES = (ETA * N) / 4U;
constexpr uint32_t ROWS = 2U * K;
constexpr uint32_t PRF_TOTAL_BYTES = ROWS * PRF_BYTES;
constexpr uint32_t SRC_COEFFS = ROWS * N;
constexpr uint32_t CBD_BLOCK_DIM = 2U;

}  // namespace F203CbdEta2Host

#if !defined(F203_CBD_BLOCK_DIM)
#define F203_CBD_BLOCK_DIM 2
#endif

#if F203_CBD_BLOCK_DIM == 1
constexpr uint32_t kHostBlockDim = 1U;
#else
constexpr uint32_t kHostBlockDim = F203CbdEta2Host::CBD_BLOCK_DIM;
#endif
