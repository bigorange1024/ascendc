/**
 * @file f203_a_hat16_config.h
 * @brief Encrypt prep 探针编译开关（与 k2 B4 已绿组件对齐）。
 *
 * 流水线相关默认（生产全量，勿改默认值绕过问题）：
 *   - F203_AHAT16_BLOCK_DIM=2：双 AIV 分片 Â
 *   - F203_AHAT16_BATCH_SHAKE=0：逐 poly SHAKE（Encrypt prep 路径）
 *   - F203_ALG7_REJ_IMPL=1：向量 rej；F203_ALG7_D12_GATHER=0
 *   - F203_CBD_BLOCK_DIM=1：PRF/CBD 仅单核（block0）
 *
 * 调试对照（须显式 -D，非默认）：F203_ALG7_XOF_504=1 等。
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
