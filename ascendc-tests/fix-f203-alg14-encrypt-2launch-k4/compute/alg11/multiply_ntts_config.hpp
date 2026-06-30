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
