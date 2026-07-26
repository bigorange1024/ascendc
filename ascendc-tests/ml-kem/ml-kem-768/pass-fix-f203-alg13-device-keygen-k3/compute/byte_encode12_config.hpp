// @probe pass-fix-f203-alg13-device-keygen-k3
// @file compute/byte_encode12_config.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `byte_encode12_config.hpp` 为该子模块组件。 / Component: byte_encode12_config.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1184B) + dk_pke.bin (1152B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends compute 树内互引；host 经 main/mmad_custom 与 tiling.h 链接。
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 行 19–20 ByteEncode₁₂：将 t̂/ŝ 编成 ek/dk polyvec。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-768（k=3）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/byte_encode12_config.hpp
 */
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
