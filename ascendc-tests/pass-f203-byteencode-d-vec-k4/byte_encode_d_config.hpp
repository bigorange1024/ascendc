/**
 * @file byte_encode_d_config.hpp
 * @brief ByteEncode_d 探针的编译期配置：选定 d（bit 宽）与向量化档位，并据此派生
 *        单 poly 输出字节数。本文件在流水线中位于 byte_encode_d_vec.hpp（核心实现）
 *        与 byte_encode_d_custom.cpp（kernel launch）之前，供两者 #include 以共享宏。
 *        与 golden 的关系：F203_BYTE_ENCODE_POLY_BYTES 需与 scripts/gen_data.py 中的
 *        OUT_BYTES 表、scripts/verify_result.py 的 nbytes 保持一致，否则对拍将因尺寸
 *        不符直接失败（尺寸为已锁定实现参数，不得擅自改动）。
 */
#ifndef BYTE_ENCODE_D_CONFIG_HPP
#define BYTE_ENCODE_D_CONFIG_HPP

/** FIPS 203 Alg.5 ByteEncode_d 的 bit 宽 d；默认 4，可在编译时通过宏覆盖。 */
#ifndef F203_BYTE_ENCODE_D
#define F203_BYTE_ENCODE_D 4
#endif

// 0=纯标量 | 1=默认验收（mask+标量 pack）| 2=真·Gather pack（保留、默认不激活；见 docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md）
#ifndef BYTE_ENCODE_D_VEC
#define BYTE_ENCODE_D_VEC 1
#endif

/** d 仅允许 4/5/10/11（对应 ML-KEM c₁/c₂ 各档 Compress 输出位宽），其余值编译期报错。 */
#if F203_BYTE_ENCODE_D != 4 && F203_BYTE_ENCODE_D != 5 && F203_BYTE_ENCODE_D != 10 && F203_BYTE_ENCODE_D != 11
#error "F203_BYTE_ENCODE_D must be 4, 5, 10, or 11"
#endif

/**
 * 单个多项式（N=256 系数）ByteEncode_d 后的输出字节数 = N*d/8：
 *   d=4  → 256*4/8  = 128B（ML-KEM-512/768 c₂）
 *   d=5  → 256*5/8  = 160B（ML-KEM-1024 c₂）
 *   d=10 → 256*10/8 = 320B（ML-KEM-512/768 c₁）
 *   d=11 → 256*11/8 = 352B（ML-KEM-1024 c₁）
 */
#if F203_BYTE_ENCODE_D == 4
#define F203_BYTE_ENCODE_POLY_BYTES 128
#elif F203_BYTE_ENCODE_D == 5
#define F203_BYTE_ENCODE_POLY_BYTES 160
#elif F203_BYTE_ENCODE_D == 10
#define F203_BYTE_ENCODE_POLY_BYTES 320
#elif F203_BYTE_ENCODE_D == 11
#define F203_BYTE_ENCODE_POLY_BYTES 352
#endif

#endif
