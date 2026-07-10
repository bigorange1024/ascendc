/**
 * @file basic.hpp
 * @brief Decrypt NTT/INTT 设备侧公共基础：引入 kernel_operator 与 LocalTensor 别名。
 *
 * 流水线位置：aic_func / aiv_func / fused entry 等统一依赖本头，
 * 避免各 TU 重复 using。无算法逻辑。
 */
#ifndef F203_BASIC_HPP
#define F203_BASIC_HPP

#include "kernel_operator.h"

using AscendC::LocalTensor;

#endif
