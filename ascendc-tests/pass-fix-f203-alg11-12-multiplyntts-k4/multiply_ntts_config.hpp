/**
 * 【文件头】Alg.11/12 MultiplyNTTs 探针的编译期开关与玩具参数。
 *
 * 本文件在流水线中的位置：被 kernel / UB / 向量路径共同 include，决定实现变体。
 * 作用：锁定 n=256、q=3329，以及 ALG11_IMPL / VEC_VARIANT / VEC_OPTS / MEM_OPS。
 * 与 golden 关系：开关只影响设备实现路径；I/O 语义仍须与 gen_data 的 Alg.11/12 golden 一致。
 */
#pragma once

/* 玩具多项式维度与模数（对齐 FIPS 203 ML-KEM n、q） */
constexpr int32_t kAlg11N = 256;
constexpr int32_t kAlg11Q = 3329;

/*
 * ALG11_IMPL：
 *   0 — UB 上标量 C（GetValue/SetValue）
 *   1 — SoA lane 上的向量 Alg.12
 *
 * ALG11_VEC_VARIANT（仅当 ALG11_IMPL=1）：
 *   1 — B1：标量 deinterleave → 向量 Mul/Add
 *   2 — B2：Gather deinterleave（共享索引）→ 向量 Mul/Add
 *
 * ALG11_VEC_OPTS（仅当 ALG11_IMPL=1）：
 *   0 — legacy：CreateVecIndex 生成 Gather 索引、标量 γ、含负值修正的 Barrett
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
