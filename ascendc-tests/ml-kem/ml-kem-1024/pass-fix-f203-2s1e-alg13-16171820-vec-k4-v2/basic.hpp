/**
 * @file basic.hpp
 * @brief 本探针 AscendC 公共薄封装：引入 kernel_operator 并导出 LocalTensor 别名。
 *
 * 流水线位置：被 aiv_func / aic_func / ntt_vec / hat_* / byte_encode* 等设备侧头文件间接包含。
 * 作用：统一 `using AscendC::LocalTensor`，避免各 hpp 重复写命名空间前缀。
 * 与 golden 关系：无独立 I/O；不参与 gen_data / verify_result。
 */
#ifndef F203_BASIC_HPP
#define F203_BASIC_HPP

#include "kernel_operator.h"

using AscendC::LocalTensor;

#endif

