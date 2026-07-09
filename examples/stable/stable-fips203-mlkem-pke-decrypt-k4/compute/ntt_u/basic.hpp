/**
 * @file basic.hpp
 * @brief Decrypt NTT/INTT 段公共前置：引入 kernel_operator 与 LocalTensor 别名。
 *
 * 被 aic_func / aiv_func / ntt_u_impl / intt_w_impl / device_fused 等包含。
 * 无算法逻辑；仅减少各 TU 重复 using。
 */
#ifndef F203_BASIC_HPP
#define F203_BASIC_HPP

#include "kernel_operator.h"

using AscendC::LocalTensor;

#endif
