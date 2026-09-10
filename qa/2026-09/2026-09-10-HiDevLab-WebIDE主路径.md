# 2026-09-10 · HiDevLab WebIDE 主路径

## 决策

- HiDevLab 云 NPU：**不以 SSH直连为主**（跳板命令约 5 分钟有效，不适合 Cloud Agent）。
- **主路径 = WebIDE + Git 配方协作**；独立手册维护，不与 CANNLab/Tailscale 手册混写。

## 落盘

- 手册：`docs/engineering/HiDevLab-WebIDE操作手册.md`
- 脚本：`scripts/hidevlab/webide_recipe.sh`（只打印可贴命令）
- 入口：`docs/engineering/INDEX.md`、`AGENTS.md`、`AGENT_HANDOFF.md`

## 环境基线（已创建）

- `DevEnv_185447`：A2 · CANN 9.1.0 ubuntu · 1 NPU 算子调测
- 实测：Ubuntu 22.04 aarch64；CANN 9.1.0；910B3；`/dev/davinci5`；逻辑设备仍 `ASCEND_DEVICE_ID=0`
- 工程目录约定：`/workspace/ascendc`

## 下一刀

WebIDE 内跑 `webide_recipe.sh add_custom` 冒烟，回传日志。
