/**
 * @file tiling.h
 * @brief Encrypt compute 段 tiling 转发头：统一 `#include "tiling.h"` → `f203_l18_l19_tiling.h`。
 *
 * 流水线位置：Alg.14 行 16–21 compute（NTT(y) / Â·ŷ / INTT）与 SIM 融合 launch `l18_l19`
 * 共用同一套 workspace 偏移与形状常量（`tiling::` 命名空间）。
 * 与 golden 关系：tiling 仅描述设备 GM/workspace 布局，不改变 `output/c.bin` 语义。
 */
// Encrypt compute tiling：Host/设备共享几何。
// 流水线：Alg.14 计算段；与 golden 仅布局一致。
// 本文件仅常量/结构，无核函数体。

#pragma once
#include "f203_l18_l19_tiling.h"
