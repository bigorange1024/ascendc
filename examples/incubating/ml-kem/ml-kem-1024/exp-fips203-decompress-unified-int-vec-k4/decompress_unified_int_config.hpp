/**
 * @file decompress_unified_int_config.hpp
 * @brief exp-fips203-decompress-unified-int-vec-k4 编译期配置（d 档位与向量/标量开关）。
 *
 * F203_UNIFIED_ROUND_D：与 golden、CMake、run.sh 一致；支持 d=1（较旧 decompress 探针新增）。
 * DECOMPRESS_UNIFIED_INT_VEC：1=默认 int32 全向量；0=标量 GetValue 对照。
 */
#ifndef DECOMPRESS_UNIFIED_INT_CONFIG_HPP
#define DECOMPRESS_UNIFIED_INT_CONFIG_HPP

#ifndef F203_UNIFIED_ROUND_D
#define F203_UNIFIED_ROUND_D 4
#endif

#ifndef DECOMPRESS_UNIFIED_INT_VEC
#define DECOMPRESS_UNIFIED_INT_VEC 1
#endif

#endif
