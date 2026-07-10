/**
 * @file basic.hpp
 * @brief Tag5T 8-poly 三段式 NTT/INTT 探针的公共 AscendC 基础头。
 *
 * 流水线位置：被 aic_func.hpp / aiv_func.hpp / ntt_vec.hpp / mmad_custom.cpp 等设备侧头共同 include。
 *
 * 作用：统一引入 `kernel_operator.h`，并 `using AscendC::LocalTensor`，避免各翻译单元重复声明。
 *
 * 与 golden 关系：无独立 I/O；仅为设备侧编译依赖，不参与 gen_data / verify 对拍。
 *
 * 语义背景：本探针为 poly-batch 紧凑 [HI₈,LO₈] 布局；本文件不定义布局常量（见 tiling.h）。
 */
#ifndef F203_BASIC_HPP
#define F203_BASIC_HPP

#include "kernel_operator.h"

using AscendC::LocalTensor;

#endif
