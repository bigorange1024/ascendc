/**
 * @file f203_compress_d_params.hpp
 * @brief FIPS 203 Compress_d 编译期参数（q=3329）。
 *
 * 本文件在流水线中的位置：位于 compress_d_config.hpp 之上，向 compress_d_vec.hpp /
 * compress_d_custom.cpp 提供按 F203_COMPRESS_D 展开的具体常数（Barrett 乘数/偏置/移位，
 * 或 cast_div 路径的开关位），不含任何运行期逻辑，纯编译期宏派生。
 * 与 golden 的关系：这些常数须与 compress_d_ref.c 中同 d 的标量实现（Barrett 乘数/移位）
 * 数学等价，否则设备输出与 golden_comp.bin 无法对拍一致。
 *
 * 向量路径选型（见 docs/notes/F203-Compress-Decompress-向量实现指南.md）：
 *   - d∈{4,5}：int32 Barrett（Muls→Adds→ShiftRight→mask）
 *   - d∈{10,11}：cast_div 商（Muls(2^d)→Adds(q/2)→Cast→Div→CAST_TRUNC→mask）
 */
#ifndef F203_COMPRESS_D_PARAMS_HPP
#define F203_COMPRESS_D_PARAMS_HPP

#include "compress_d_config.hpp"

// d=4：int32 Barrett 路径。乘数/偏置/移位与 compress_d_ref.c::f203_scalar_compress_d4 一致：
// round(16u/q) = (u*1290160 + 2^27) >> 28，无需再 mod（移位后天然落在 [0,15]）。
#if F203_COMPRESS_D == 4
#define F203_COMPRESS_USE_CAST_DIV 0
#define F203_COMPRESS_D_BITS 4
#define F203_COMPRESS_BARRETT_MUL 1290160
#define F203_COMPRESS_BARRETT_BIAS (1 << 27)
#define F203_COMPRESS_BARRETT_SHIFT 28
// d=5：int32 Barrett 路径，移位后需再 mask 低 5 位（对应 compress_d_vec.hpp 中 mask_low_bits_i32）。
#elif F203_COMPRESS_D == 5
#define F203_COMPRESS_USE_CAST_DIV 0
#define F203_COMPRESS_D_BITS 5
#define F203_COMPRESS_BARRETT_MUL 1290176
#define F203_COMPRESS_BARRETT_BIAS (1 << 26)
#define F203_COMPRESS_BARRETT_SHIFT 27
// d=10：乘数超出 u32 安全范围，改走 cast_div（浮点除法求商）路径，不再定义 Barrett 常数。
#elif F203_COMPRESS_D == 10
#define F203_COMPRESS_USE_CAST_DIV 1
#define F203_COMPRESS_D_BITS 10
// d=11：同 d=10，走 cast_div 路径。
#elif F203_COMPRESS_D == 11
#define F203_COMPRESS_USE_CAST_DIV 1
#define F203_COMPRESS_D_BITS 11
#else
#error "F203_COMPRESS_D must be 4, 5, 10, or 11"
#endif

#endif
