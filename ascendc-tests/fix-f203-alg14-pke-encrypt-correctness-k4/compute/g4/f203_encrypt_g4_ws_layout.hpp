/**
 * @file f203_encrypt_g4_ws_layout.hpp
 * @brief G4 紧凑 workspace 布局（单 GM 块，供 2 参 launch 规避 SIM 507000）。
 */
#pragma once

#include "f203_encrypt_layout.h"
#include "f203_ntt_r_tiling.h"

namespace f203_g4_ws {

constexpr size_t kUOff = 0U;
constexpr size_t kTrOff = tiling::dstFileBytes;
constexpr size_t kE1Off = kTrOff + tiling::dstFileBytes;
constexpr size_t kE2Off = kE1Off + F203_E1_POLYVEC_BYTES;
constexpr size_t kMOff = kE2Off + F203_E2_POLY_BYTES;
constexpr size_t kTotalBytes = kMOff + F203_MSG_BYTES;

} // namespace f203_g4_ws
