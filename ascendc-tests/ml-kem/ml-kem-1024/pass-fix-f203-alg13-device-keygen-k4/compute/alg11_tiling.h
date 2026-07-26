// @probe pass-fix-f203-alg13-device-keygen-k4
// @file compute/alg11_tiling.h
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `alg11_tiling.h` 为该子模块组件。 / Component: alg11_tiling.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: cstdint
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 行 18 Alg.11 basemul / γ 表与向量管线。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/alg11_tiling.h
 */
/**
 * @file alg11_tiling.h
 * @brief Alg.11 向量 basemul 的静态尺寸：N、pairCount、VecWs 槽位数。
 *
 * 用途：为 multiply_ntts_vec / hat_alg11 提供编译期常量；ALG11_MEM_OPS 决定 ws 是否含内嵌索引 LUT。
 *
 * 调用方：multiply_ntts_ub.hpp、multiply_ntts_vec.hpp、hat_dot_ub_tiling.hpp（kVecWsInts 与之对齐）。
 *
 * 不变量：kN=256、kPairCount=128；kBlockDim=1（本探针单核 MIX）；MEM_OPS=1 时 kVecWsInts=8*128。
 *
 * Golden：无直接对拍；经行 18 t_hat 间接验收。
 *
 * CMake：ALG11_MEM_OPS（multiply_ntts_config.hpp）。
 */
#pragma once

#include <cstdint>

#ifndef ALG11_MEM_OPS
#define ALG11_MEM_OPS 1
#endif

namespace alg11_tiling {
constexpr int32_t kN = 256;
constexpr int32_t kBlockDim = 1;
constexpr int32_t kPairCount = kN / 2;
constexpr int32_t kInterleaveReorderCount = kN;
#if ALG11_MEM_OPS == 1
/** ROM 索引独立 Init UB；ws 仅 a0..t2 */
constexpr int32_t kVecWsInts = 8 * kPairCount;
#else
constexpr int32_t kVecWsInts = 10 * kPairCount;
#endif
}  // namespace alg11_tiling
