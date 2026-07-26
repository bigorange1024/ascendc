/**
 * 【文件头】Alg.11/12 MultiplyNTTs 探针的 tiling / 工作区尺寸常量。
 *
 * 本文件在流水线中的位置：host（main）与 device（kernel）共用的几何参数。
 * 作用：定义 n=256、单核 blockDim、pair 数、向量工作区 int 个数（随 MEM_OPS 变化）。
 * 与 golden 关系：golden 亦为长度 256 的 int32 多项式；此处尺寸须与 gen_data 一致。
 */
#pragma once

#include <cstdint>

#ifndef ALG11_MEM_OPS
#define ALG11_MEM_OPS 1
#endif

namespace alg11_tiling {
/** 多项式系数个数（FIPS ML-KEM n） */
constexpr int32_t kN = 256;
/** 本探针仅用 1 个 AIV */
constexpr int32_t kBlockDim = 1;
/** Alg.12 基域乘法对数：n/2 = 128 */
constexpr int32_t kPairCount = kN / 2;
/** interleave 重排索引个数（整条 poly，256） */
constexpr int32_t kInterleaveReorderCount = kN;
#if ALG11_MEM_OPS == 1
/** ROM 索引独立 Init UB；ws 仅 a0..t2 共 8 条 lane × pairCount */
constexpr int32_t kVecWsInts = 8 * kPairCount;
#else
/** legacy：ws 另含 idx/idx2 两条索引 lane，共 10 × pairCount */
constexpr int32_t kVecWsInts = 10 * kPairCount;
#endif
}  // namespace alg11_tiling
