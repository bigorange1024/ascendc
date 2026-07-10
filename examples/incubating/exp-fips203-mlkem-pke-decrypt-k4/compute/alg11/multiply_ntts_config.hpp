/**
 * @file multiply_ntts_config.hpp
 * @brief Alg.11/12 实现开关（Decrypt su_dot / MultiplyNTTs）。
 *
 * 生产默认：ALG11_IMPL=1（向量）、VEC_VARIANT=2（Gather deinterleave）、MEM_OPS=1（ROM DataCopy）。
 * 调试对照须显式 -D 覆盖；禁止把调试值写成默认。
 */
#pragma once

/** 多项式长度与模数（FIPS ML-KEM n=256, q=3329）。 */
constexpr int32_t kAlg11N = 256;
constexpr int32_t kAlg11Q = 3329;

/*
 * ALG11_IMPL:
 *   0 — UB 上标量 C（GetValue/SetValue）
 *   1 — 向量 Alg.12（SoA lane）
 *
 * ALG11_VEC_VARIANT（仅 ALG11_IMPL=1）：
 *   1 — B1：标量 deinterleave → 向量 Mul/Add
 *   2 — B2：Gather deinterleave（共享索引）→ 向量 Mul/Add
 *
 * ALG11_VEC_OPTS（仅 ALG11_IMPL=1）：
 *   0 — legacy：CreateVecIndex Gather 索引、标量 γ、含负值修正 Barrett
 *   1 — §9 优化：固定 n 索引填表、Duplicate γ、reduce_zq_vec_barrett_basemul
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
 * ALG11_MEM_OPS（ALG11_IMPL=1）：
 *   0 — legacy：CreateVecIndex 索引、SetValue γ、标量 interleave
 *   1 — __gm__ ROM + Init DataCopy；Gather 索引导入 UB；interleave 用 DataCopy+Gather
 */
#ifndef ALG11_MEM_OPS
#define ALG11_MEM_OPS 1
#endif
