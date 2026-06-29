#ifndef BYTE_ENCODE12_CONFIG_HPP
#define BYTE_ENCODE12_CONFIG_HPP

/**
 * ByteEncode₁₂ 实现变体（CMake -DBYTE_ENCODE12_VEC / -DBYTE_ENCODE12_SCATTER_VEC 覆盖）。
 *
 * BYTE_ENCODE12_VEC:
 *   0 — 标量 poly_byte_encode12_scalar（与 2s1e 原实现一致）
 *   1 — 向量：Gather 解交错 + uint16 And/Shift + tile=32（默认）
 *
 * BYTE_ENCODE12_SCATTER_VEC:
 *   0 — SoA→AoS 交织用标量 SetValue
 *   1 — int32 打包（4 pair→3 word）+ DataCopy 96B/tile（910B4 无 Scatter API）
 */
#ifndef BYTE_ENCODE12_VEC
#define BYTE_ENCODE12_VEC 1
#endif

#ifndef BYTE_ENCODE12_SCATTER_VEC
#define BYTE_ENCODE12_SCATTER_VEC 1
#endif

#endif
