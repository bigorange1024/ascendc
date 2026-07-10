/**
 * @file f203_mlkem_params.h
 * @brief FIPS 203（ML-KEM）全局标量参数，供本探针（pass-f203-byteencode-d-vec-k4，
 *        ByteEncode_d 向量实现）host/device 两侧共用。
 *        仅提供与具体 d（bit 宽）无关的算法常量；d 相关的输出字节数等派生常量见
 *        byte_encode_d_config.hpp。
 */
#ifndef F203_MLKEM_PARAMS_H
#define F203_MLKEM_PARAMS_H

#include <stdint.h>

/** ML-KEM 模数 q=3329（本探针未直接参与 ByteEncode_d 计算，仅作跨探针统一声明保留）。 */
#define F203_MLKEM_Q 3329
/** 单个多项式的系数个数 N=256（FIPS 203 §2.4），即 ByteEncode_d 一次处理的系数数量。 */
#define F203_MLKEM_N 256

#endif
