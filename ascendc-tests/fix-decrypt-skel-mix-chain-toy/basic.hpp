/**
 * @file basic.hpp
 * @brief 通用基础头：引入 AscendC kernel_operator.h 并把常用类型 LocalTensor
 * 提升到全局命名空间，供本探针（pass-toy-mix-s123-byteencode-k2）各设备侧
 * 文件（aic_func.hpp / aiv_func.hpp 等）统一 include，避免重复写
 * `AscendC::LocalTensor`。不含任何计算逻辑，纯粹的编译期便利头。
 */
#ifndef F203_BASIC_HPP
#define F203_BASIC_HPP

#include "kernel_operator.h"

using AscendC::LocalTensor;

#endif
