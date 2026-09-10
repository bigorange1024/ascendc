# Agent 交接 — 每日刷新

> **最后刷新**：2026-09-10（HiDevLab 真机改走 **WebIDE 主路径**；独立手册已落盘）  
> 分支：`cursor/hidevlab-cloud-npu-9099`

## 真机分流

| 环境 | 主路径 | 文档 |
|------|--------|------|
| **HiDevLab**（当前） | **WebIDE + Git 配方**（Agent 不长 SSH） | [`docs/engineering/HiDevLab-WebIDE操作手册.md`](docs/engineering/HiDevLab-WebIDE操作手册.md) |
| GitCode CANNLab | Tailscale + `scripts/cannlab/` | [`docs/engineering/CANNLab接入与远程驱动.md`](docs/engineering/CANNLab接入与远程驱动.md) |

HiDevLab **SSH直连**约 5 分钟票 → **非** Agent 主通道。

## 当前环境基线（DevEnv_185447）

- A2 · CANN 9.1.0 ubuntu · 910B3（物理 NPU 5 / 逻辑 `ASCEND_DEVICE_ID=0`）
- 工程：`/workspace/ascendc`（曾部署本分支；以 Git 为准）
- 配方：`bash scripts/hidevlab/webide_recipe.sh add_custom`

## P0

人侧 WebIDE：`git pull` → 粘贴 `webide_recipe.sh` 冒烟 `add_custom -r npu` → 回传日志。
