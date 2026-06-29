/**
 * @file launch_profile.h
 * @brief exp-mlkem-f203-keygen-k4 launch 规模（customspec §AI Core）。
 * Prep：AIV_ONLY aiv=2；Compute：vec-k4-v2 MIX aicore=1。
 */
#pragma once
#include <cstdint>
namespace ExpMlkemF203Keygen {
constexpr uint32_t kPrepAivBlockDim = 2U;
constexpr uint32_t kComputeMixBlockDim = 1U;
}
