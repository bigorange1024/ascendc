#ifndef INTEGRATION_CONFIG_HPP
#define INTEGRATION_CONFIG_HPP

#include "multiply_ntts_config.hpp"

/** 行 18：1=Alg11 向量 basemul（B2+MEM_OPS=1）；0=标量 multiply_ntts_half_scalar */
#ifndef HAT_ALG11_VEC
#define HAT_ALG11_VEC 1
#endif

#ifndef BYTE_ENCODE12_VEC
#define BYTE_ENCODE12_VEC 1
#endif

#ifndef BYTE_ENCODE12_SCATTER_VEC
#define BYTE_ENCODE12_SCATTER_VEC 1
#endif

namespace hat_alg11_cfg {
constexpr uint32_t kRomInt32Slots = 4U * 128U;       // γ + gatherEven + gatherOdd + interleave(256)
constexpr uint32_t kBasemulWsInts = 8U * 128U;
constexpr uint32_t kGammaSliceInts = 128U;
constexpr uint32_t kExtraInt32Slots = kRomInt32Slots + kBasemulWsInts + kGammaSliceInts;
constexpr uint32_t kHatPcMult = 8U; // f,g,t1,t2 区相对 basemul ws 的 pc 倍数（与标量 10 解耦）
} // namespace hat_alg11_cfg

#endif
