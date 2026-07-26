/**
 * @file tiling.h
 * @brief Stage1–3 / AIV 兼容入口：转发至本探针正式 tiling 头。
 *
 * 流水线位置：`aiv_func.hpp`、`ntt_vec.hpp` 等历史路径 `#include "tiling.h"`；
 * 实际常量与 workspace 布局定义在 `f203_l18_l19_tiling.h`（Alg.14 行 2/18/19/21，kP=5、INTT k=8）。
 *
 * 禁止在本文件另写几何常量，以免与 `f203_l18_l19_tiling.h` 漂移。
 */
#pragma once
#include "f203_l18_l19_tiling.h"
