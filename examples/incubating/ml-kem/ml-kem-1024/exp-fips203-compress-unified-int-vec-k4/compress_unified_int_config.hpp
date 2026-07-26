/**
 * @file compress_unified_int_config.hpp
 * @brief exp-fips203-compress-unified-int-vec-k4 编译期配置（d 档位与向量/标量开关）。
 *
 * F203_UNIFIED_ROUND_D：与 golden ref.c、CMake CACHE、run.sh 环境变量须一致。
 * COMPRESS_UNIFIED_INT_VEC：1=生产 int32 limb 向量；0=int64 lane 标量对照（SIM tick 基准）。
 */
#ifndef COMPRESS_UNIFIED_INT_CONFIG_HPP
#define COMPRESS_UNIFIED_INT_CONFIG_HPP

#ifndef F203_UNIFIED_ROUND_D
#define F203_UNIFIED_ROUND_D 4
#endif

#ifndef COMPRESS_UNIFIED_INT_VEC
#define COMPRESS_UNIFIED_INT_VEC 1
#endif

#endif
