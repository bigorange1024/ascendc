/**
 * @file tiling.h
 * @brief Encrypt compute 段 tiling 转发头：统一 `#include "tiling.h"` → `f203_l18_l19_tiling.h`。
 *
 * 流水线位置：Alg.14 行 16–21 compute（NTT(y) / Â·ŷ / INTT）与 SIM 融合 launch `l18_l19`
 * 共用同一套 workspace 偏移与形状常量（`tiling::` 命名空间）。
 * 与 golden 关系：tiling 仅描述设备 GM/workspace 布局，不改变 `output/c.bin` 语义。
 */
#pragma once
#include "f203_l18_l19_tiling.h"
