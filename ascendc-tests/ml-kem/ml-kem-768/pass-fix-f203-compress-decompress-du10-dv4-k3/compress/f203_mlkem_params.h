/**
 * @file f203_mlkem_params.h
 * @brief ML-KEM（FIPS 203）全局数学常数，供本探针 host/device 两侧共享。
 *
 * 本文件在流水线中的位置：位于所有 compress_d_* 头文件的最底层，无其它内部依赖；
 * 与规范的关系：q、n 直接取自 FIPS 203 §2.3（模数 q=3329、每个多项式系数个数 n=256），
 * 不随 d 变化，本探针与 golden（compress_d_ref.c）共用同一份常数定义。
 */
#ifndef F203_MLKEM_PARAMS_H
#define F203_MLKEM_PARAMS_H

#include <stdint.h>

#define F203_MLKEM_Q 3329  // ML-KEM 模数 q（FIPS 203 §2.3），系数环 Z_q
#define F203_MLKEM_N 256   // 每个多项式系数个数 n（FIPS 203 §2.3）

#endif
