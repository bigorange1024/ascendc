/**
 * @file multiply_ntts_config.hpp
 * @brief Alg.11 MultiplyNTTs 编译开关（实现变体 / 向量优化 / 内存路径）。
 *
 * 流水线位置：被 multiply_ntts_vec / su_dot 在编译期选择代码路径。
 * 生产默认：ALG11_IMPL=1、VEC_VARIANT=2、MEM_OPS=1（全量向量 + ROM）。
 * 调试对照须显式 -D 覆盖，并标非默认（见仓库「默认=全量」规则）。
 */
#pragma once

/* 多项式维数与模数（ml_kem_1024 / FIPS q） */
constexpr int32_t kAlg11N = 256;
constexpr int32_t kAlg11Q = 3329;

/*
 * ALG11_IMPL:
 *   0 — UB 标量 GetValue/SetValue（对照）
 *   1 — 向量 Alg.12 SoA 路径（生产）
 *
 * ALG11_VEC_VARIANT（仅 IMPL=1）:
 *   1 — B1：标量 deinterleave → 向量 Mul/Add
 *   2 — B2：Gather deinterleave（共享索引）→ 向量 Mul/Add（生产）
 *
 * ALG11_VEC_OPTS（仅 IMPL=1）:
 *   0 — 遗留：CreateVecIndex / 标量 γ / 含负值修正 Barrett
 *   1 — §9：固定 n 索引、Duplicate γ、basemul Barrett
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
 * ALG11_MEM_OPS（IMPL=1）:
 *   0 — 遗留：CreateVecIndex、SetValue γ、标量 interleave
 *   1 — 生产：__gm__ ROM + Init DataCopy；Gather 索引进 UB；interleave DataCopy+Gather
 */
#ifndef ALG11_MEM_OPS
#define ALG11_MEM_OPS 1
#endif
