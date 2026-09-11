# Agent 交接 — 每日刷新

> **最后刷新**：2026-09-11（HiDevLab：人侧 `webide_start.sh` + Agent 侧 `agent_connect.sh`；连接说明已落盘）  
> 分支：`cursor/hidevlab-webide-start-ffc6`

## 真机分流

| 环境 | 主路径 | 文档 |
|------|--------|------|
| **HiDevLab**（当前） | 人：WebIDE 跑 `webide_start.sh`；Agent：Cursor 上 `agent_connect.sh` | **[`docs/engineering/HiDevLab-Agent连接.md`](docs/engineering/HiDevLab-Agent连接.md)**（短文）· [`HiDevLab-WebIDE操作手册.md`](docs/engineering/HiDevLab-WebIDE操作手册.md) |
| GitCode CANNLab | Tailscale + `scripts/cannlab/` | [`docs/engineering/CANNLab接入与远程驱动.md`](docs/engineering/CANNLab接入与远程驱动.md) |

HiDevLab **WebIDE 常不能 git** → 人侧脚本放 `/workspace/user_data/`；**Agent 在 Cursor 机器上 git pull 本仓**即可拿连接脚本。  
SSH直连约 5 分钟票 → **非** Agent 主通道。

## 其他 Agent 连服务器（复制即用）

```bash
# Cursor Agent 仓库根
git fetch origin cursor/hidevlab-webide-start-ffc6 && git checkout cursor/hidevlab-webide-start-ffc6
bash scripts/hidevlab/agent_connect.sh
bash scripts/hidevlab/agent_connect.sh --exec 'hostname; npu-smi info | head'
```

Secret：`TAILSCALE_AUTHKEY`（或 `TS_AUTHKEY`）+ `CANNLAB_SSH_KEY`（坏格式可用，脚本会 materialize）。

人侧每次开机（WebIDE）：

```bash
TS_AUTHKEY='…' bash /workspace/user_data/webide_start.sh
```

## 当前环境基线（DevEnv_185447）

- A2 · CANN 9.1.0 ubuntu · 910B3/910B4（逻辑 `ASCEND_DEVICE_ID=0`）
- 工程：`/workspace/ascendc`（盒子常无 git；以 Cursor 仓为准）
- 配方：`bash scripts/hidevlab/webide_recipe.sh add_custom`

## P0

- HiDevLab 连接链路：人 `webide_start` + Agent `agent_connect`（见上）
- 私钥兜底：`scripts/hidevlab/materialize_ssh_key.sh`（**勿再改 Dashboard Secret**）
- 开机冒烟仍可用：`bash /workspace/ascendc/scripts/hidevlab/webide_boot.sh` + 配方
