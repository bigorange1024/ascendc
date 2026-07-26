/**
 * @file byte_decode_d_config.hpp
 * @brief ByteDecode_d 探针的编译期配置：选定 d（bit 宽）与向量化档位，并据此派生
 *        单 poly 输入字节数。本文件在流水线中位于 byte_decode_d_vec.hpp（核心实现）
 *        与 byte_decode_d_custom.cpp（kernel launch）之前，供两者 #include 以共享宏。
 *        与 golden 的关系：F203_BYTE_DECODE_POLY_BYTES 需与 scripts/gen_data.py 中的
 *        IN_BYTES 表保持一致（golden 由本目录 byte_decode_d_ref.c 与上游
 *        byteencode 目录的 byte_encode_d_ref.c round-trip 生成），否则对拍将因尺寸
 *        不符直接失败（尺寸为已锁定实现参数，不得擅自改动）。
 */
#ifndef BYTE_DECODE_D_CONFIG_HPP
#define BYTE_DECODE_D_CONFIG_HPP

/** FIPS 203 Alg.6 ByteDecode_d 的 bit 宽 d；默认 4，可在编译时通过宏覆盖。 */
#ifndef F203_BYTE_DECODE_D
#define F203_BYTE_DECODE_D 4
#endif

// 0=标量 | 1=默认（d=4 向量 nibble；d=5/10/11 标量 unpack，与 0 同体）；见 docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md
#ifndef BYTE_DECODE_D_VEC
#define BYTE_DECODE_D_VEC 1
#endif

/** d 仅允许 4/5/10/11（对应 ML-KEM c₁/c₂ 各档 Compress 输出位宽），其余值编译期报错。 */
#if F203_BYTE_DECODE_D != 4 && F203_BYTE_DECODE_D != 5 && F203_BYTE_DECODE_D != 10 && F203_BYTE_DECODE_D != 11
#error "F203_BYTE_DECODE_D must be 4, 5, 10, or 11"
#endif

/**
 * 单个多项式（N=256 系数）ByteDecode_d 的输入字节数 = N*d/8（与上游 ByteEncode_d
 * 输出字节数一致，二者互为逆变换）：
 *   d=4  → 256*4/8  = 128B（ML-KEM-512/768 c₂）
 *   d=5  → 256*5/8  = 160B（ML-KEM-1024 c₂）
 *   d=10 → 256*10/8 = 320B（ML-KEM-512/768 c₁）
 *   d=11 → 256*11/8 = 352B（ML-KEM-1024 c₁）
 */
#if F203_BYTE_DECODE_D == 4
#define F203_BYTE_DECODE_POLY_BYTES 128
#elif F203_BYTE_DECODE_D == 5
#define F203_BYTE_DECODE_POLY_BYTES 160
#elif F203_BYTE_DECODE_D == 10
#define F203_BYTE_DECODE_POLY_BYTES 320
#elif F203_BYTE_DECODE_D == 11
#define F203_BYTE_DECODE_POLY_BYTES 352
#endif

#endif
