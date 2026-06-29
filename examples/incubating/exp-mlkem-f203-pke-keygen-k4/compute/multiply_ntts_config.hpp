// @probe exp-mlkem-f203-pke-keygen-k4
// @file compute/multiply_ntts_config.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `multiply_ntts_config.hpp` 为该子模块组件。 / Component: multiply_ntts_config.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends compute 树内互引；host 经 main/mmad_custom 与 tiling.h 链接。
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。

/**
 * @file multiply_ntts_config.hpp
 * @brief FIPS 203 Alg.11 MultiplyNTTs 的设备实现选型宏（标量 vs 向量、B1/B2、ROM 导入）。
 *
 * 用途：编译期选择 basemul 后端：标量 GetValue/SetValue、向量 SoA 车道、Gather 解交错、GM ROM DataCopy 等。
 *
 * 调用方：被 `multiply_ntts_ub.hpp`、`multiply_ntts_vec.hpp`、`integration_config.hpp` 间接包含；
 *         行 18 经 `alg11_ub::compute_on_ub` 或 `hat_alg11::multiply_ntts_half_vec` 进入。
 *
 * 不变量：n=256、q=3329；pairCount=128；γ 表与 alg11_gammas.h / hat_gammas.hpp 同步。
 *
 * Golden：hat_inner_product_ref.c 标量 Alg.11；verify 对拍 t_hat 系数（非逐 basemul）。
 *
 * CMake（CMakeLists.txt CACHE，经 cpu_lib/npu_lib 传入内核）：
 *   ALG11_IMPL、ALG11_VEC_VARIANT、ALG11_VEC_OPTS、ALG11_MEM_OPS
 */
#pragma once

/* Toy polynomial dimension and modulus */
constexpr int32_t kAlg11N = 256;
constexpr int32_t kAlg11Q = 3329;

/*
 * ALG11_IMPL:
 *   0 — scalar C on UB (GetValue/SetValue)
 *   1 — vector Alg.12 on SoA lanes
 *
 * ALG11_VEC_VARIANT (only when ALG11_IMPL=1):
 *   1 — B1: scalar deinterleave → vec Mul/Add
 *   2 — B2: Gather deinterleave (shared index) → vec Mul/Add
 *
 * ALG11_VEC_OPTS (only when ALG11_IMPL=1):
 *   0 — legacy: CreateVecIndex Gather 索引、标量 γ、含负值修正 Barrett
 *   1 — §9 优化: 固定 n 索引填表、Duplicate γ、reduce_zq_vec_barrett_basemul
 */
#ifndef ALG11_IMPL
#define ALG11_IMPL 1
#endif

#ifndef ALG11_VEC_VARIANT
#define ALG11_VEC_VARIANT 2
#endif

#ifndef ALG11_VEC_OPTS
#define ALG11_VEC_OPTS 0
#endif

/*
 * ALG11_MEM_OPS (ALG11_IMPL=1):
 *   0 — legacy：CreateVecIndex 索引、SetValue γ、标量 interleave
 *   1 — __gm__ ROM + Init DataCopy；Gather 索引导入 UB；interleave 用 DataCopy+Gather
 */
#ifndef ALG11_MEM_OPS
#define ALG11_MEM_OPS 1
#endif
