/**
 * @file compress_d_config.hpp
 * @brief pass-f203-compress-d-vec-k4 探针的编译期开关总入口。
 *
 * 本文件在流水线中的位置：位于所有 Compress_d 相关头文件（f203_compress_d_params.hpp、
 * compress_d_vec.hpp）的最底层依赖，仅负责把 -D 编译宏（`F203_COMPRESS_D`、`COMPRESS_D_VEC`）
 * 归一化为带默认值的宏，并对非法 `F203_COMPRESS_D` 取值做编译期报错；不包含任何计算逻辑。
 * 对齐规范：FIPS 203 §4.2.1 Compress_d，探针验收 d∈{4,5,10,11}（见 IMPLEMENTATION_PLAN.md）。
 * 与 golden 的关系：`F203_COMPRESS_D`/`COMPRESS_D_VEC` 由 run.sh 透传给 host/device 编译与
 * scripts/gen_data.py（环境变量 `F203_COMPRESS_D`），三方须取值一致才能保证 kernel 输出与
 * compress_d_ref.c 生成的 golden_comp.bin 对拍成立。
 */
#ifndef COMPRESS_D_CONFIG_HPP
#define COMPRESS_D_CONFIG_HPP

// F203_COMPRESS_D：压缩位宽 d；未在编译命令行指定时默认取 4（最常用于 c2/ByteEncode_4 场景）。
#ifndef F203_COMPRESS_D
#define F203_COMPRESS_D 4
#endif

// 0=标量 fallback | 1=默认向量 per-lane（tail 抄此）；见 docs/notes/F203-Compress-Decompress-向量实现指南.md
// COMPRESS_D_VEC 控制 compress_d_vec.hpp 中 poly_compress_local 的实现路径选择：
// 默认 1（向量 Barrett/cast_div），0 时退化为逐系数标量 fallback，仅用于对照验证。
#ifndef COMPRESS_D_VEC
#define COMPRESS_D_VEC 1
#endif

// 仅接受 FIPS 203 Encrypt/Decrypt 用到的四个压缩位宽；其余取值在编译期直接报错，避免
// 后续 f203_compress_d_params.hpp 落入未定义分支。
#if F203_COMPRESS_D != 4 && F203_COMPRESS_D != 5 && F203_COMPRESS_D != 10 && F203_COMPRESS_D != 11
#error "F203_COMPRESS_D must be 4, 5, 10 (ML-KEM-768 u), or 11 (ML-KEM-1024 u)"
#endif

#endif
