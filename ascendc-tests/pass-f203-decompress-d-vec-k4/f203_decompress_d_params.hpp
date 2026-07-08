/**
 * @file f203_decompress_d_params.hpp
 * @brief FIPS 203 Decompress_d 编译期参数：bias=2^{d-1}，shift=d。
 *
 * 全档 d 均为 int32 向量：Muls(q)→Adds(bias)→ShiftRight(d)。
 */
#ifndef F203_DECOMPRESS_D_PARAMS_HPP
#define F203_DECOMPRESS_D_PARAMS_HPP

#include "decompress_d_config.hpp"

#if F203_DECOMPRESS_D == 4
#define F203_DECOMPRESS_D_BITS 4
#define F203_DECOMPRESS_ROUND_BIAS 8
#elif F203_DECOMPRESS_D == 5
#define F203_DECOMPRESS_D_BITS 5
#define F203_DECOMPRESS_ROUND_BIAS 16
#elif F203_DECOMPRESS_D == 10
#define F203_DECOMPRESS_D_BITS 10
#define F203_DECOMPRESS_ROUND_BIAS 512
#elif F203_DECOMPRESS_D == 11
#define F203_DECOMPRESS_D_BITS 11
#define F203_DECOMPRESS_ROUND_BIAS 1024
#else
#error "F203_DECOMPRESS_D must be 4, 5, 10, or 11"
#endif

#endif
