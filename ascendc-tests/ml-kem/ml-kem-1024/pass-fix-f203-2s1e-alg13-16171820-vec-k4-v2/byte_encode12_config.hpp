/**
 * @file byte_encode12_config.hpp
 * @brief FIPS 203 Alg.5 ByteEncode₁₂ 设备实现选型（标量 vs 向量、交织写回策略）。
 *
 * 用途：行 19–20 将 mod q 后的 int32 多项式压成 384B/poly（12bit×256 系数 → 3B/对）。
 *
 * 调用方：byte_encode12_pair.hpp、byte_encode12_vec.hpp、`2s1e_post_ntt_ub.hpp`（HAT_BYTE_ENCODE=1）。
 *
 * 不变量：每 poly 384 字节；系数取低 12 bit；与 byte_encode12_ref.c 位级语义一致。
 *
 * Golden：gen_data.py 经 libbyte_encode12_ref.so 生成 golden_ek/sk；verify_result.py cmp ek_out/sk_out。
 *
 * CMake：BYTE_ENCODE12_VEC、BYTE_ENCODE12_SCATTER_VEC、BYTE_ENCODE12_PREFETCH。
 * 本文件仅编译期宏，无运行时函数体。
 */
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
 *
 * BYTE_ENCODE12_PREFETCH:
 *   0 — tile=32：每 tile DataCopy+CreateVecIndex+Gather×2（legacy）
 *   1 — 整 poly：ROM 索引 + 1×Gather×2 + 128-wide 向量算 + 384B pack（默认）
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
