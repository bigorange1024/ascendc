/**
 * 设备侧 ROM 常量表（__gm__），由 Init 一次 DataCopy 进 UB，禁止 Compute 热路径 SetValue。
 */
#ifndef ALG11_ROM_TABLES_H
#define ALG11_ROM_TABLES_H

#include "alg11_gammas.h"
#include "kernel_operator.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel_operator.h"

extern __gm__ const int32_t gAlg11GammasGm[ALG11_PAIR_COUNT];
extern __gm__ const int32_t gAlg11GatherEvenByteGm[ALG11_PAIR_COUNT];
extern __gm__ const int32_t gAlg11GatherOddByteGm[ALG11_PAIR_COUNT];
extern __gm__ const int32_t gAlg11InterleaveReorderByteGm[ALG11_PAIR_COUNT * 2];

#ifdef __cplusplus
}
#endif

#endif
