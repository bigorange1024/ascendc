#ifndef BYTE_ENCODE12_CONFIG_HPP
#define BYTE_ENCODE12_CONFIG_HPP

/**
 * @file byte_encode12_config.hpp
 * @brief ByteEncode₁₂ 探针编译期开关（CMake -D 覆盖）。
 *
 * 流水线位置：被 pair / vec / only / custom 等设备侧头文件包含，决定标量/向量/prefetch 路径。
 * 与 golden 关系：仅影响设备实现路径；I/O 语义须与 byte_encode12_ref / gen_data golden 一致。
 *
 * BYTE_ENCODE12_VEC:
 *   0 — 标量 poly_byte_encode12_scalar（与 2s1e 原实现一致）
 *   1 — 向量：Gather 解交错 + uint16 And/Shift + tile=32（默认）
 *
 * BYTE_ENCODE12_SCATTER_VEC:
 *   0 — SoA→AoS 交织用标量 SetValue
 *   1 — int32 打包（4 pair→3 word）+ DataCopy 96B/tile（910B4 无 Scatter API）
 *
 * BYTE_ENCODE12_PREFETCH:
 *   0 — tile=32：每 tile DataCopy+CreateVecIndex+Gather×2（legacy）
 *   1 — 整 poly：Init ROM 索引 + 1×Gather×2 + 128-wide 向量算 + 384B pack（默认实验路径）
 */
#ifndef BYTE_ENCODE12_VEC
#define BYTE_ENCODE12_VEC 1
#endif

#ifndef BYTE_ENCODE12_SCATTER_VEC
#define BYTE_ENCODE12_SCATTER_VEC 1
#endif

#ifndef BYTE_ENCODE12_PREFETCH
#define BYTE_ENCODE12_PREFETCH 1
#endif

#endif
