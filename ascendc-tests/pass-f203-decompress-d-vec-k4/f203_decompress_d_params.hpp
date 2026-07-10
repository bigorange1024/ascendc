/**
 * @file f203_decompress_d_params.hpp
 * @brief FIPS 203 Decompress_d 编译期参数：bias=2^{d-1}，shift=d。
 *
 * 本文件在流水线中的位置：位于 decompress_d_config.hpp 之上，向 decompress_d_vec.hpp /
 * decompress_d_custom.cpp 提供按 F203_DECOMPRESS_D 展开的具体常数（四舍五入偏置 bias 与
 * 右移位宽 shift），不含运行期逻辑，纯编译期宏派生。
 * 与 golden 的关系：这些常数须与 decompress_d_ref.c 中同 d 分支的 bias/shift 数学等价，
 * 否则设备输出与 golden_poly.bin 无法逐系数对拍一致。
 *
 * 全档 d 均为 int32 向量：Muls(q)→Adds(bias)→ShiftRight(d)。
 */
#ifndef F203_DECOMPRESS_D_PARAMS_HPP
#define F203_DECOMPRESS_D_PARAMS_HPP

#include "decompress_d_config.hpp"

// d=4：bias=2^3=8，对应 Decompress_4(u) = (u*q + 8) >> 4。
#if F203_DECOMPRESS_D == 4
#define F203_DECOMPRESS_D_BITS 4
#define F203_DECOMPRESS_ROUND_BIAS 8
// d=5：bias=2^4=16，对应 Decompress_5(u) = (u*q + 16) >> 5。
#elif F203_DECOMPRESS_D == 5
#define F203_DECOMPRESS_D_BITS 5
#define F203_DECOMPRESS_ROUND_BIAS 16
// d=10：bias=2^9=512，对应 Decompress_10(u) = (u*q + 512) >> 10。
#elif F203_DECOMPRESS_D == 10
#define F203_DECOMPRESS_D_BITS 10
#define F203_DECOMPRESS_ROUND_BIAS 512
// d=11：bias=2^10=1024，对应 Decompress_11(u) = (u*q + 1024) >> 11。
#elif F203_DECOMPRESS_D == 11
#define F203_DECOMPRESS_D_BITS 11
#define F203_DECOMPRESS_ROUND_BIAS 1024
#else
#error "F203_DECOMPRESS_D must be 4, 5, 10, or 11"
#endif

#endif
