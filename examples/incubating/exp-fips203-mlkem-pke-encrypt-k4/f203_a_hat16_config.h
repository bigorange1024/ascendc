/**
 * @file f203_a_hat16_config.h
 * @brief Encrypt prep 编译开关（SampleNTT / Â 分片 / CBD），与 KeyGen 对齐。
 *
 * 流水线位置：Alg.14 行 3–15 prep；默认值即生产全量路径，调试改宏须显式覆盖。
 * 与 golden：仅影响设备实现路径选择，不改变 I/O 语义。
 */
#pragma once

#ifndef F203_ALG7_REJ_IMPL
/** 1=向量拒绝采样（生产默认）；0=标量对照。 */
#define F203_ALG7_REJ_IMPL 1
#endif

#ifndef F203_ALG7_D12_GATHER
/** 0=禁止 Gather 解 d12（生产）；非 0 为调试路径。 */
#define F203_ALG7_D12_GATHER 0
#endif

#ifndef F203_AHAT16_BLOCK_DIM
/** Â 双 AIV：2。 */
#define F203_AHAT16_BLOCK_DIM 2
#endif

#ifndef F203_AHAT16_BATCH_SHAKE
/** 0=逐 poly SHAKE（生产）；1=批 SHAKE 实验。 */
#define F203_AHAT16_BATCH_SHAKE 0
#endif

#ifndef F203_ALG7_XOF_504
/** 0=标准 672B XOF；504 为压缩实验（非默认）。 */
#define F203_ALG7_XOF_504 0
#endif

#ifndef F203_CBD_BLOCK_DIM
/** CBD 仅 block0 串行：1。 */
#define F203_CBD_BLOCK_DIM 1
#endif
