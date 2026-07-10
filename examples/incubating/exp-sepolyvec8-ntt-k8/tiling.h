/**
 * @file tiling.h
 * @brief sepolyvec8 NTT Host/设备共享几何与 workspace 偏移。
 *
 * 流水线位置：main 读 tiling.bin（tileLength,kPolys）；kernel 用 tiling::M0..A1 切 ws。
 * 布局：LUT 四块 M0..M3 各 [256,256] int8；S0 为 Stage1 后 A[16,256] int8；A0/A1 为 Stage2 积。
 */
#ifndef __TIILING_H__
#define __TIILING_H__
#include <cstddef>
#include <cstdint>

/** Host↔设备传递的最小 tiling：系数长度与 poly 批大小。 */
struct TilingData {
    int32_t tileLength;  // 通常 = n = 256
    int32_t kPolys;      // 本探针固定 8
};

namespace tiling {
constexpr size_t n = 256;
constexpr size_t kPolys = 8;
/** split 后左矩阵 A [16,256] int8，行序 [hi0,lo0,…,hi7,lo7] */
constexpr size_t mRows = 2 * kPolys;

constexpr size_t M0 = 0;
constexpr size_t M1 = M0 + n * n;
constexpr size_t M2 = M1 + n * n;
constexpr size_t M3 = M2 + n * n;

constexpr size_t S0 = M3 + n * n;
constexpr size_t S2 = S0 + mRows * n;
constexpr size_t S3 = S2 + n;
constexpr size_t A0 = S3 + n;
constexpr size_t A1 = A0 + mRows * n * sizeof(int32_t);
constexpr size_t wssize = A1 + mRows * n * sizeof(int32_t);
} // namespace tiling

#endif
