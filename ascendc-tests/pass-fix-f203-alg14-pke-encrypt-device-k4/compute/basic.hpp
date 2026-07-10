/**
 * @file basic.hpp
 * @brief Alg.14 Encrypt compute 段公共前置头：AscendC 算子头与 LocalTensor 别名。
 *
 * 流水线位置：被 aic_func / aiv_func / 各 MIX kernel 间接或直接包含；
 * 本身不含算法逻辑，仅统一 `#include "kernel_operator.h"` 与 `using`。
 *
 * 与 golden：无 I/O；golden 对拍由各 kernel + host `main.cpp` / `scripts/gen_data.py` 负责。
 */
#ifndef F203_BASIC_HPP
#define F203_BASIC_HPP

#include "kernel_operator.h"

/** 设备侧局部张量类型别名，缩短 aic/aiv 模板中的书写 */
using AscendC::LocalTensor;

#endif
