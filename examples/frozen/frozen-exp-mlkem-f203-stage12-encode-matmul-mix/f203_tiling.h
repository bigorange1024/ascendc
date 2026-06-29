#ifndef F203_TILING_H
#define F203_TILING_H

#include <cstddef>
#include <cstdint>

namespace f203_ws {
constexpr size_t N = 256;
constexpr size_t M_ROWS = 16;
constexpr size_t OUT_COLS = 512;

/** LUT [256,512] int8 连续存放 */
constexpr size_t LUT = 0;
constexpr size_t MAT_A = LUT + N * OUT_COLS;
constexpr size_t WSSIZE = MAT_A + M_ROWS * N;
} // namespace f203_ws

#endif
