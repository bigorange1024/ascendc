#ifndef __TIILING_H__
#define __TIILING_H__
#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t tileLength;
    int32_t kPolys;
    /** 0=融合 S1+S2；1=仅 Stage1 Split（SIM/CPU 分段 launch Stage2 时用） */
    int32_t mixPass;
};

namespace tiling {
constexpr size_t n = 256;
constexpr size_t kPolys = 8;
constexpr size_t outCols = 512;
constexpr size_t mRows = 2 * kPolys;

/** LUT B [256,512] int8 = M0‖M1 */
constexpr size_t B_LUT = 0;
constexpr size_t S0 = B_LUT + n * outCols;
constexpr size_t S2 = S0 + mRows * n;
constexpr size_t S3 = S2 + n;
constexpr size_t wssize = S3 + n;
} // namespace tiling

#endif
