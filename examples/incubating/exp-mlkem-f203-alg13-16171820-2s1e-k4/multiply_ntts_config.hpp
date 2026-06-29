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
