/**
 * @file f203_compress_d_params.hpp
 * @brief FIPS 203 Compress_d 编译期参数（q=3329）。
 *
 * 向量路径选型（见 docs/notes/F203-Compress-Decompress-向量实现指南.md）：
 *   - d∈{4,5}：int32 Barrett（Muls→Adds→ShiftRight→mask）
 *   - d∈{10,11}：cast_div 商（Muls(2^d)→Adds(q/2)→Cast→Div→CAST_TRUNC→mask）
 */
#ifndef F203_COMPRESS_D_PARAMS_HPP
#define F203_COMPRESS_D_PARAMS_HPP

#include "compress_d_config.hpp"

#if F203_COMPRESS_D == 4
#define F203_COMPRESS_USE_CAST_DIV 0
#define F203_COMPRESS_D_BITS 4
#define F203_COMPRESS_BARRETT_MUL 1290160
#define F203_COMPRESS_BARRETT_BIAS (1 << 27)
#define F203_COMPRESS_BARRETT_SHIFT 28
#elif F203_COMPRESS_D == 5
#define F203_COMPRESS_USE_CAST_DIV 0
#define F203_COMPRESS_D_BITS 5
#define F203_COMPRESS_BARRETT_MUL 1290176
#define F203_COMPRESS_BARRETT_BIAS (1 << 26)
#define F203_COMPRESS_BARRETT_SHIFT 27
#elif F203_COMPRESS_D == 10
#define F203_COMPRESS_USE_CAST_DIV 1
#define F203_COMPRESS_D_BITS 10
#elif F203_COMPRESS_D == 11
#define F203_COMPRESS_USE_CAST_DIV 1
#define F203_COMPRESS_D_BITS 11
#else
#error "F203_COMPRESS_D must be 4, 5, 10, or 11"
#endif

#endif
