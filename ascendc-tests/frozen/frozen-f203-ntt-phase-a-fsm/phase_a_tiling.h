#ifndef F203_PHASE_A_TILING_H
#define F203_PHASE_A_TILING_H

#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength;
};

namespace phase_a_tiling {
// 对齐 merged_kyber wssize，供 MIX auto_gen matmul::clearWorkspace 使用
constexpr size_t kWsBytes = 262144;
} // namespace phase_a_tiling

#endif
