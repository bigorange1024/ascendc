#ifndef MLKEM_NTT_BATCH_REF_H
#define MLKEM_NTT_BATCH_REF_H

#include <stdint.h>

/** 对 k 行 Z_q 多项式逐条 MlkemNtt；in/out 均为 [k,256] int32 row-major。 */
void mlkem_ntt_batch_ref(int32_t *out, const int32_t *in, int k_polys);

#endif
