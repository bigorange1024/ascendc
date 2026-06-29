/**
 * @file f203_a_hat16_config.h
 * @brief 16-poly Â 探针编译开关（与单 poly alg7 config 对齐）。
 */
#pragma once

#ifndef F203_ALG7_REJ_IMPL
#define F203_ALG7_REJ_IMPL 1
#endif

#ifndef F203_ALG7_D12_GATHER
#define F203_ALG7_D12_GATHER 0
#endif

/** 1=单 AIV 跑满 16 poly（默认）；2=双 AIV 各 8 poly（polyIdx 0–7 / 8–15）。 */
#ifndef F203_AHAT16_BLOCK_DIM
#define F203_AHAT16_BLOCK_DIM 2
#endif

/**
 * 1=一次 batch16 SHAKE（x 行 64B stride；SIM 可跑但 tick **负优化** ~960k vs 逐条 ~734k）。
 * 0=16×逐条 SHAKE（**默认**，SIM 基线）。
 */
#ifndef F203_AHAT16_BATCH_SHAKE
#define F203_AHAT16_BATCH_SHAKE 0
#endif

/**
 * 1=SHAKE 仅 squeeze 504B（3×rate）；0=672B 默认。
 * 须与 golden ROM 同步（run.sh 导出 F203_ALG7_XOF_504 再 gen ROM / gen_data）。
 */
#ifndef F203_ALG7_XOF_504
#define F203_ALG7_XOF_504 0
#endif
