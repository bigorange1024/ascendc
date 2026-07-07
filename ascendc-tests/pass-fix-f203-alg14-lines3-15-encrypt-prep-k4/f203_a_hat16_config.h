/**
 * @file f203_a_hat16_config.h
 * @brief Encrypt prep 探针编译开关（与 stable KeyGen / pass lines3-7 对齐）。
 */
#pragma once

#ifndef F203_ALG7_REJ_IMPL
#define F203_ALG7_REJ_IMPL 1
#endif

#ifndef F203_ALG7_D12_GATHER
#define F203_ALG7_D12_GATHER 0
#endif

#ifndef F203_AHAT16_BLOCK_DIM
#define F203_AHAT16_BLOCK_DIM 2
#endif

#ifndef F203_AHAT16_BATCH_SHAKE
#define F203_AHAT16_BATCH_SHAKE 0
#endif

#ifndef F203_ALG7_XOF_504
#define F203_ALG7_XOF_504 0
#endif

#ifndef F203_CBD_BLOCK_DIM
#define F203_CBD_BLOCK_DIM 1
#endif
